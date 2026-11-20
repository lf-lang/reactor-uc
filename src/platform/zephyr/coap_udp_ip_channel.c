/**
 * CoAP-over-UDP federated network channel built on Zephyr's CoAP packet library.
 *
 * Port of src/platform/riot/coap_udp_ip_channel.c. The protocol on the wire is identical:
 * every federate runs a CoAP server with the resources /connect, /disconnect and /message and
 * talks to its peers with confirmable POST requests. Peers are identified by their IP address.
 *
 * Threads / execution contexts:
 *  - Runtime thread:            open_connection, send_blocking, close_connection, ...
 *  - Zephyr CoAP server thread: server resource handlers (incl. the receive callback)
 *  - Client thread (ours):      receives ACKs, retransmits, sends /connect requests
 *
 * Client: the requests are built with the CoAP packet API and sent on ONE UDP socket shared by all channels.
 * Zephyr's coap_client is not used: it polls its socket with a fixed timeout of 500 ms and only checks for due
 * retransmissions after that, so a lost message or ACK delayed the retransmission by up to 500 ms (instead of the
 * ACK timeout), and its request entries are a global pool of full request copies.
 * Our client thread polls the socket with the time until the next retransmission as timeout. An eventfd wakes it
 * up when another thread starts a request (new deadline).
 *
 * Send queue: every channel queues up to COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE serialized messages. Only the oldest
 * one is in flight (confirmable /message request); the next one is sent when its ACK arrives. One message in flight
 * per channel keeps the order per peer and makes duplicate detection by message ID sufficient. send_blocking only
 * waits if the queue is full, so a retransmission does not block the runtime as long as the queue has room.
 * /connect and /disconnect use the same in-flight slot: a channel connects while its queue is empty (not
 * connected) and disconnects after its queue was acknowledged or dropped.
 *
 * Memory:
 *  - Per channel: CoapUdpIpChannel incl. its send queue ((COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE + 2) per slot).
 *  - Global: one socket, one eventfd, the client thread (stack + one transmit buffer).
 *    Incoming messages are decoded into a FederateMessage on the stack of the CoAP server thread (see the /message
 *    handler), so CONFIG_COAP_SERVER_STACK_SIZE must hold sizeof(FederateMessage).
 *
 * Channels are found through the bundles of the federated environment (by remote address for incoming requests and
 * ACKs, round-robin for retransmissions and connect attempts).
 *
 * Locking: `coap_lock` protects all channel fields that change at runtime. It is never held while sending a packet
 * (sending may wait for network buffers) or while calling the receive callback.
 */
#include "reactor-uc/platform/zephyr/coap_udp_ip_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/serialization.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/coap_service.h>
#include <zephyr/random/random.h>
#include <zephyr/zvfs/eventfd.h>

#define COAP_UDP_IP_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)

#define COAP_PATH_CONNECT "connect"
#define COAP_PATH_DISCONNECT "disconnect"
#define COAP_PATH_MESSAGE "message"

#define COAP_IS_SUCCESS(code) (((code) >> 5) == 2) // response class 2.xx

// Largest request on the wire
#define COAP_UDP_IP_CHANNEL_PACKET_SIZE (COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE + COAP_UDP_IP_CHANNEL_HEADER_SIZE)

// Datagrams on the client socket are ACKs and resets (4 bytes, we use no token). Longer ones are truncated.
#define COAP_UDP_IP_CHANNEL_RX_SIZE 16

#if CONFIG_COAP_SERVER_MESSAGE_SIZE < COAP_UDP_IP_CHANNEL_PACKET_SIZE
#error                                                                                                                 \
    "CONFIG_COAP_SERVER_MESSAGE_SIZE must hold COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE + COAP_UDP_IP_CHANNEL_HEADER_SIZE"
#endif

/** The confirmable request a channel has in flight (CoapUdpIpChannel.tx_kind). */
typedef enum {
  COAP_REQUEST_NONE,
  COAP_REQUEST_CONNECT,
  COAP_REQUEST_DISCONNECT,
  COAP_REQUEST_MESSAGE,
} CoapUdpIpChannelRequestKind;

/* ------------------------------------------------------------------------------------------------- */
/* Global state                                                                                       */
/* ------------------------------------------------------------------------------------------------- */

static int coap_sock = -1;    // client socket of all channels
static int coap_wake_fd = -1; // eventfd: wakes up the client thread when another thread started a request
static size_t coap_connects_in_flight = 0;
static size_t coap_cursor = 0; // bundle index where the client thread's round-robin search starts
static bool coap_globals_initialized = false;
static bool coap_globals_init_ok = false;

static K_MUTEX_DEFINE(coap_lock);
static K_CONDVAR_DEFINE(coap_cond); // signalled whenever a queue slot becomes free or a channel changes its state

static struct k_thread coap_client_thread;
static K_THREAD_STACK_DEFINE(coap_client_stack, COAP_UDP_IP_CHANNEL_CLIENT_STACK_SIZE);
// Only used by the client thread (retransmissions and connect requests); other threads build on their stack.
static uint8_t coap_client_tx_buf[COAP_UDP_IP_CHANNEL_PACKET_SIZE];

#if COAP_UDP_IP_CHANNEL_STATS
static uint32_t _CoapUdpIpChannel_us_since(int64_t ticks) {
  uint64_t us = k_ticks_to_us_floor64((uint64_t)(k_uptime_ticks() - ticks));
  return us > UINT32_MAX ? UINT32_MAX : (uint32_t)us;
}

/** Sort @p us into the buckets of @p hist (see COAP_UDP_IP_CHANNEL_STATS) and update @p max. */
static void _CoapUdpIpChannel_stats_record(uint32_t* hist, uint32_t* max, uint32_t us) {
  uint32_t t = coap_get_transmission_parameters().ack_timeout * 1000U; // us
  const uint32_t bounds[COAP_UDP_IP_CHANNEL_STATS_BUCKETS - 1] = {t / 3, t, 3 * t, 7 * t, 15 * t};
  size_t i = 0;
  while (i < COAP_UDP_IP_CHANNEL_STATS_BUCKETS - 1 && us > bounds[i]) {
    i++;
  }
  hist[i]++;
  if (us > *max) {
    *max = us;
  }
}
#endif

/* ------------------------------------------------------------------------------------------------- */
/* Helpers                                                                                            */
/* ------------------------------------------------------------------------------------------------- */

/** Milliseconds from @p now until @p deadline (both k_uptime_get_32()), negative if it passed. Wrap-around safe. */
static int32_t _CoapUdpIpChannel_ms_until(uint32_t deadline, uint32_t now) { return (int32_t)(deadline - now); }

static bool _CoapUdpIpChannel_addr_equal(const struct sockaddr* a, const struct sockaddr* b) {
  if (a->sa_family != b->sa_family) {
    return false;
  }
#if defined(CONFIG_NET_IPV4)
  if (a->sa_family == AF_INET) {
    return memcmp(&((const struct sockaddr_in*)a)->sin_addr, &((const struct sockaddr_in*)b)->sin_addr,
                  sizeof(struct in_addr)) == 0;
  }
#endif
#if defined(CONFIG_NET_IPV6)
  if (a->sa_family == AF_INET6) {
    return memcmp(&((const struct sockaddr_in6*)a)->sin6_addr, &((const struct sockaddr_in6*)b)->sin6_addr,
                  sizeof(struct in6_addr)) == 0;
  }
#endif
  return false;
}

static socklen_t _CoapUdpIpChannel_addr_len(const struct sockaddr* addr) {
  return addr->sa_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
}

static size_t _CoapUdpIpChannel_num_bundles(void) {
  FederatedEnvironment* env = (FederatedEnvironment*)_lf_environment;
  return env != NULL && env->net_bundles != NULL ? env->net_bundles_size : 0;
}

/** Returns the CoAP channel of bundle @p i, or NULL if the bundle is not constructed yet or uses another channel. */
static CoapUdpIpChannel* _CoapUdpIpChannel_of_bundle(size_t i) {
  FederatedConnectionBundle* bundle = ((FederatedEnvironment*)_lf_environment)->net_bundles[i];
  if (bundle == NULL || bundle->net_channel == NULL || bundle->net_channel->type != NETWORK_CHANNEL_TYPE_COAP_UDP_IP) {
    return NULL;
  }
  return (CoapUdpIpChannel*)bundle->net_channel;
}

/** Must be called with coap_lock held. */
static CoapUdpIpChannel* _CoapUdpIpChannel_find_by_addr_locked(const struct sockaddr* addr) {
  for (size_t i = 0; i < _CoapUdpIpChannel_num_bundles(); i++) {
    CoapUdpIpChannel* ch = _CoapUdpIpChannel_of_bundle(i);
    if (ch != NULL && _CoapUdpIpChannel_addr_equal(&ch->remote, addr)) {
      return ch;
    }
  }
  return NULL;
}

/** Wake up the client thread, so that it takes a request started by another thread into account. */
static void _CoapUdpIpChannel_wake_client(void) {
  if (zvfs_eventfd_write(coap_wake_fd, 1) < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to wake up the client thread (%d)", errno);
  }
}

/**
 * Must be called with coap_lock held. Drops the queued messages (incl. the one in flight) and wakes up senders
 * waiting for a queue slot. A /connect or /disconnect in flight is kept.
 */
static void _CoapUdpIpChannel_flush_queue_locked(CoapUdpIpChannel* self) {
  if (self->tx_kind == COAP_REQUEST_MESSAGE) {
    self->tx_kind = COAP_REQUEST_NONE;
  }
#if COAP_UDP_IP_CHANNEL_STATS
  self->stats.dropped += self->queue_count;
#endif
  self->queue_count = 0;
  k_condvar_broadcast(&coap_cond);
}

/**
 * Update the state of a channel. Must be called with coap_lock held.
 * Informs the runtime if the channel became connected or disconnected. A channel that is not connected drops its
 * queued messages (they were meant for the previous connection).
 */
static void _CoapUdpIpChannel_set_state_locked(CoapUdpIpChannel* self, NetworkChannelState new_state) {
  NetworkChannelState old_state = self->state;
  if (old_state == new_state) {
    return;
  }
  COAP_UDP_IP_CHANNEL_DEBUG("Update state: %s => %s", NetworkChannel_state_to_string(old_state),
                            NetworkChannel_state_to_string(new_state));
  self->state = new_state;
  if (new_state != NETWORK_CHANNEL_STATE_CONNECTED) {
    _CoapUdpIpChannel_flush_queue_locked(self);
  }
  k_condvar_broadcast(&coap_cond);

  if ((old_state == NETWORK_CHANNEL_STATE_CONNECTED) != (new_state == NETWORK_CHANNEL_STATE_CONNECTED)) {
    _lf_environment->platform->notify(_lf_environment->platform);
  }
}

static bool _CoapUdpIpChannel_needs_connect_locked(const CoapUdpIpChannel* self) {
  if (self->tx_kind != COAP_REQUEST_NONE) {
    return false;
  }
  switch (self->state) {
  case NETWORK_CHANNEL_STATE_OPEN:
  case NETWORK_CHANNEL_STATE_CONNECTION_FAILED:
  case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
    return true;
  default:
    return false;
  }
}

/**
 * Transmission parameters of a request: Zephyr's current defaults (an application can change them with
 * coap_set_transmission_parameters). /connect and /disconnect retransmit less often (see
 * COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT).
 */
static struct coap_transmission_parameters _CoapUdpIpChannel_transmission_params(uint8_t kind) {
  struct coap_transmission_parameters params = coap_get_transmission_parameters();
  if (kind != COAP_REQUEST_MESSAGE) {
#ifdef COAP_UDP_IP_CHANNEL_CONNECT_ACK_TIMEOUT_MS
    params.ack_timeout = COAP_UDP_IP_CHANNEL_CONNECT_ACK_TIMEOUT_MS;
#endif
    params.max_retransmission = COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT;
  }
  return params;
}

/** Initial ACK timeout: random in [ack_timeout, ack_timeout * ack_random_percent / 100] (RFC 7252, 4.8). */
static uint32_t _CoapUdpIpChannel_initial_timeout(const struct coap_transmission_parameters* params) {
  uint32_t timeout = params->ack_timeout;
#if defined(CONFIG_COAP_RANDOMIZE_ACK_TIMEOUT)
  uint16_t random_percent = params->ack_random_percent ? params->ack_random_percent : CONFIG_COAP_ACK_RANDOM_PERCENT;
  uint32_t max_timeout = params->ack_timeout * random_percent / 100U;
  if (max_timeout > timeout) {
    timeout += sys_rand32_get() % (max_timeout - timeout + 1);
  }
#endif
  return timeout;
}

/**
 * Build a POST request into @p buf (COAP_UDP_IP_CHANNEL_PACKET_SIZE bytes). We use no token: the ACK is matched by
 * its message ID and the peer address, and every response is piggybacked.
 * Returns the length of the packet or a negative errno.
 */
static int _CoapUdpIpChannel_build_request(uint8_t* buf, uint8_t type, uint16_t mid, const char* path,
                                           const uint8_t* payload, size_t payload_len) {
  struct coap_packet pkt;
  int ret = coap_packet_init(&pkt, buf, COAP_UDP_IP_CHANNEL_PACKET_SIZE, COAP_VERSION_1, type, 0, NULL,
                             COAP_METHOD_POST, mid);
  if (ret == 0) {
    ret = coap_packet_append_option(&pkt, COAP_OPTION_URI_PATH, (const uint8_t*)path, strlen(path));
  }
  if (ret == 0 && payload_len > 0) {
    ret = coap_append_option_int(&pkt, COAP_OPTION_CONTENT_FORMAT, COAP_CONTENT_FORMAT_APP_OCTET_STREAM);
    if (ret == 0) {
      ret = coap_packet_append_payload_marker(&pkt);
    }
    if (ret == 0) {
      ret = coap_packet_append_payload(&pkt, payload, payload_len);
    }
  }
  if (ret < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to build CoAP request /%s (%d)", path, ret);
    return ret;
  }
  return pkt.offset;
}

/** Must be called with coap_lock held. Build the request in flight of @p self into @p buf (see build_request). */
static int _CoapUdpIpChannel_build_in_flight_locked(const CoapUdpIpChannel* self, uint8_t* buf) {
  switch (self->tx_kind) {
  case COAP_REQUEST_CONNECT:
    return _CoapUdpIpChannel_build_request(buf, COAP_TYPE_CON, self->tx_mid, COAP_PATH_CONNECT, NULL, 0);
  case COAP_REQUEST_DISCONNECT:
    return _CoapUdpIpChannel_build_request(buf, COAP_TYPE_CON, self->tx_mid, COAP_PATH_DISCONNECT, NULL, 0);
  case COAP_REQUEST_MESSAGE: {
    const CoapUdpIpChannelQueuedMessage* msg = &self->queue[self->queue_head];
    return _CoapUdpIpChannel_build_request(buf, COAP_TYPE_CON, self->tx_mid, COAP_PATH_MESSAGE, msg->data, msg->len);
  }
  default:
    return -EINVAL;
  }
}

/**
 * Must be called with coap_lock held. Start the confirmable request @p kind (for COAP_REQUEST_MESSAGE: the head of
 * the queue) and build its first transmission into @p buf. The caller sends it after releasing the lock.
 * Returns the length of the packet or a negative errno.
 */
static int _CoapUdpIpChannel_start_request_locked(CoapUdpIpChannel* self, uint8_t kind, uint8_t* buf) {
  struct coap_transmission_parameters params = _CoapUdpIpChannel_transmission_params(kind);
  self->tx_kind = kind;
  self->tx_mid = coap_next_id(); // not thread safe => only called with coap_lock held
  self->tx_transmissions = 1;
  self->tx_timeout_ms = _CoapUdpIpChannel_initial_timeout(&params);
  self->tx_deadline_ms = k_uptime_get_32() + self->tx_timeout_ms;
  if (kind == COAP_REQUEST_CONNECT) {
    coap_connects_in_flight++;
  }
#if COAP_UDP_IP_CHANNEL_STATS
  if (kind == COAP_REQUEST_MESSAGE) {
    self->stats.sent_con++;
    self->stats.sent_ticks = k_uptime_ticks();
  }
#endif
  return _CoapUdpIpChannel_build_in_flight_locked(self, buf);
}

/** Must be called with coap_lock held. The request in flight is finished (answered, failed or replaced). */
static void _CoapUdpIpChannel_end_request_locked(CoapUdpIpChannel* self) {
  if (self->tx_kind == COAP_REQUEST_CONNECT) {
    coap_connects_in_flight--;
  }
  self->tx_kind = COAP_REQUEST_NONE;
}

/** Send a packet on the client socket. Must be called WITHOUT coap_lock held. */
static int _CoapUdpIpChannel_send_packet(const uint8_t* buf, int len, const struct sockaddr* remote) {
  if (zsock_sendto(coap_sock, buf, (size_t)len, 0, remote, _CoapUdpIpChannel_addr_len(remote)) < 0) {
    int err = errno;
    COAP_UDP_IP_CHANNEL_ERR("Failed to send CoAP packet (%d)", err);
    return -err;
  }
  return 0;
}

/* ------------------------------------------------------------------------------------------------- */
/* Client thread                                                                                      */
/* ------------------------------------------------------------------------------------------------- */

/**
 * Must be called with coap_lock held. The request in flight of @p self was answered with an ACK (code @p code) or a
 * reset (@p reset). Returns the length of the next request it built into coap_client_tx_buf (the next queued
 * message), or 0.
 */
static int _CoapUdpIpChannel_handle_answer_locked(CoapUdpIpChannel* self, bool reset, uint8_t code) {
  bool success = !reset && COAP_IS_SUCCESS(code);
  uint8_t kind = self->tx_kind;
  _CoapUdpIpChannel_end_request_locked(self);

  switch (kind) {
  case COAP_REQUEST_CONNECT:
    COAP_UDP_IP_CHANNEL_DEBUG("Connect response: %d", reset ? -1 : code);
    if (self->state == NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS) {
      if (success) {
        _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTED);
      } else {
        COAP_UDP_IP_CHANNEL_WARN("Connect failed (%d) => try again", reset ? -1 : code);
        _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
        self->tx_deadline_ms = k_uptime_get_32() + COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS;
      }
    }
    return 0;

  case COAP_REQUEST_DISCONNECT:
    COAP_UDP_IP_CHANNEL_DEBUG("Disconnect response: %d", reset ? -1 : code);
    return 0;

  case COAP_REQUEST_MESSAGE:
    if (!success) {
#if COAP_UDP_IP_CHANNEL_STATS
      self->stats.failed++;
#endif
      COAP_UDP_IP_CHANNEL_ERR("Message was rejected (%d) => lost connection", reset ? -1 : code);
      if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
        _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
        self->tx_deadline_ms = k_uptime_get_32(); // reconnect right away
      }
      return 0;
    }
#if COAP_UDP_IP_CHANNEL_STATS
    self->stats.acked++;
    _CoapUdpIpChannel_stats_record(self->stats.ack_us, &self->stats.ack_max_us,
                                   _CoapUdpIpChannel_us_since(self->stats.sent_ticks));
#endif
    self->queue_head = (self->queue_head + 1) % COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE;
    self->queue_count--;
    k_condvar_broadcast(&coap_cond);
    if (self->queue_count > 0) {
      return _CoapUdpIpChannel_start_request_locked(self, COAP_REQUEST_MESSAGE, coap_client_tx_buf);
    }
    return 0;

  default:
    return 0;
  }
}

/** A datagram arrived on the client socket: match ACKs and resets with the request in flight of the sender. */
static void _CoapUdpIpChannel_handle_datagram(const uint8_t* buf, int len, const struct sockaddr* from) {
  struct coap_packet pkt;
  if (coap_packet_parse(&pkt, (uint8_t*)buf, (uint16_t)len, NULL, 0) < 0) {
    return;
  }
  uint8_t type = coap_header_get_type(&pkt);
  if (type != COAP_TYPE_ACK && type != COAP_TYPE_RESET) {
    return; // nobody sends requests to the client socket
  }
  uint16_t mid = coap_header_get_id(&pkt);
  uint8_t code = coap_header_get_code(&pkt);

  struct sockaddr remote = {0};
  int next_len = 0;
  k_mutex_lock(&coap_lock, K_FOREVER);
  CoapUdpIpChannel* self = _CoapUdpIpChannel_find_by_addr_locked(from);
  if (self != NULL) {
    if (self->tx_kind != COAP_REQUEST_NONE && self->tx_mid == mid) {
      next_len = _CoapUdpIpChannel_handle_answer_locked(self, type == COAP_TYPE_RESET, code);
      remote = self->remote;
    } else {
#if COAP_UDP_IP_CHANNEL_STATS
      self->stats.late_acks++;
#endif
    }
  }
  k_mutex_unlock(&coap_lock);

  if (next_len > 0) {
    (void)_CoapUdpIpChannel_send_packet(coap_client_tx_buf, next_len, &remote);
  }
}

/**
 * Must be called with coap_lock held. The request in flight of @p self got no ACK after its last retransmission.
 */
static void _CoapUdpIpChannel_handle_timeout_locked(CoapUdpIpChannel* self, uint32_t now) {
  uint8_t kind = self->tx_kind;
  _CoapUdpIpChannel_end_request_locked(self);
  switch (kind) {
  case COAP_REQUEST_CONNECT:
    COAP_UDP_IP_CHANNEL_DEBUG("Connect timed out");
    if (self->state == NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS) {
      _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
      self->tx_deadline_ms = now + COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS;
    }
    break;
  case COAP_REQUEST_DISCONNECT:
    COAP_UDP_IP_CHANNEL_DEBUG("Disconnect timed out");
    break;
  case COAP_REQUEST_MESSAGE:
#if COAP_UDP_IP_CHANNEL_STATS
    self->stats.failed++;
#endif
    COAP_UDP_IP_CHANNEL_ERR("Message was not acknowledged => lost connection");
    if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
      _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
      self->tx_deadline_ms = now; // reconnect right away
    }
    break;
  default:
    break;
  }
}

/**
 * Must be called with coap_lock held. Find the next due transmission (round-robin over all channels): a
 * retransmission or a connect attempt. Builds it into coap_client_tx_buf, copies the receiver to @p remote and
 * returns its length. Returns 0 if nothing is due; then @p wait_ms is the time until the next deadline (-1: none).
 */
static int _CoapUdpIpChannel_next_due_locked(struct sockaddr* remote, int32_t* wait_ms) {
  uint32_t now = k_uptime_get_32();
  int32_t wait = -1;
  size_t num_bundles = _CoapUdpIpChannel_num_bundles();

  for (size_t k = 0; k < num_bundles; k++) {
    size_t i = (coap_cursor + k) % num_bundles;
    CoapUdpIpChannel* self = _CoapUdpIpChannel_of_bundle(i);
    if (self == NULL) {
      continue;
    }
    int len = 0;
    int32_t left = _CoapUdpIpChannel_ms_until(self->tx_deadline_ms, now);

    if (self->tx_kind != COAP_REQUEST_NONE) {
      if (left > 0) {
        wait = wait < 0 || left < wait ? left : wait;
        continue;
      }
      struct coap_transmission_parameters params = _CoapUdpIpChannel_transmission_params(self->tx_kind);
      if (self->tx_transmissions > params.max_retransmission) {
        _CoapUdpIpChannel_handle_timeout_locked(self, now);
        left = _CoapUdpIpChannel_ms_until(self->tx_deadline_ms, now); // a failed connection may reconnect now
      } else {
        self->tx_transmissions++;
        self->tx_timeout_ms = self->tx_timeout_ms * params.coap_backoff_percent / 100U;
        self->tx_deadline_ms = now + self->tx_timeout_ms;
#if COAP_UDP_IP_CHANNEL_STATS
        if (self->tx_kind == COAP_REQUEST_MESSAGE) {
          self->stats.retransmissions++;
        }
#endif
        len = _CoapUdpIpChannel_build_in_flight_locked(self, coap_client_tx_buf);
      }
    }

    if (len == 0 && _CoapUdpIpChannel_needs_connect_locked(self)) {
      if (left > 0) {
        wait = wait < 0 || left < wait ? left : wait;
        continue;
      }
      if (coap_connects_in_flight >= COAP_UDP_IP_CHANNEL_CONNECT_WINDOW) {
        continue; // a finished connect request frees a place (handled in this thread, which then scans again)
      }
      COAP_UDP_IP_CHANNEL_DEBUG("Sending connect request");
      _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS);
      len = _CoapUdpIpChannel_start_request_locked(self, COAP_REQUEST_CONNECT, coap_client_tx_buf);
    }

    if (len != 0) {
      coap_cursor = i + 1;
      if (len < 0) {
        // Cannot be built (does not happen with valid sizes): give up on this request.
        _CoapUdpIpChannel_handle_timeout_locked(self, now);
        *wait_ms = 0;
        return 0;
      }
      *remote = self->remote;
      return len;
    }
  }
  *wait_ms = wait;
  return 0;
}

static void _CoapUdpIpChannel_client_thread(void* p1, void* p2, void* p3) {
  (void)p1;
  (void)p2;
  (void)p3;
  uint8_t rx[COAP_UDP_IP_CHANNEL_RX_SIZE];

  while (true) {
    // Send everything that is due
    int32_t wait_ms;
    struct sockaddr remote;
    while (true) {
      k_mutex_lock(&coap_lock, K_FOREVER);
      int len = _CoapUdpIpChannel_next_due_locked(&remote, &wait_ms);
      k_mutex_unlock(&coap_lock);
      if (len <= 0) {
        if (wait_ms != 0) {
          break;
        }
        continue;
      }
      (void)_CoapUdpIpChannel_send_packet(coap_client_tx_buf, len, &remote);
    }

    // Wait for ACKs, a wake-up or the next deadline
    struct zsock_pollfd fds[2] = {
        {.fd = coap_sock, .events = ZSOCK_POLLIN},
        {.fd = coap_wake_fd, .events = ZSOCK_POLLIN},
    };
    int ret = zsock_poll(fds, 2, wait_ms);
    if (ret < 0) {
      COAP_UDP_IP_CHANNEL_ERR("Client thread: poll failed (%d)", errno);
      k_msleep(10);
      continue;
    }
    if (fds[1].revents & ZSOCK_POLLIN) {
      zvfs_eventfd_t value;
      (void)zvfs_eventfd_read(coap_wake_fd, &value);
    }
    if (fds[0].revents & ZSOCK_POLLIN) {
      struct sockaddr from;
      socklen_t from_len = sizeof(from);
      int len;
      while ((len = zsock_recvfrom(coap_sock, rx, sizeof(rx), ZSOCK_MSG_DONTWAIT, &from, &from_len)) > 0) {
        _CoapUdpIpChannel_handle_datagram(rx, len, &from);
        from_len = sizeof(from);
      }
    }
  }
}

/* ------------------------------------------------------------------------------------------------- */
/* Server side (Zephyr CoAP server thread)                                                            */
/* Returning a response code makes the Zephyr CoAP server send a piggybacked ACK with that code.      */
/* ------------------------------------------------------------------------------------------------- */

static int _CoapUdpIpChannel_server_connect_handler(struct coap_resource* resource, struct coap_packet* request,
                                                    struct sockaddr* addr, socklen_t addr_len) {
  (void)resource;
  (void)request;
  (void)addr_len;
  COAP_UDP_IP_CHANNEL_DEBUG("Server connect handler");
  int code;

  k_mutex_lock(&coap_lock, K_FOREVER);
  CoapUdpIpChannel* self = _CoapUdpIpChannel_find_by_addr_locked(addr);
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server connect handler: Client has unknown IP address");
    code = COAP_RESPONSE_CODE_UNAUTHORIZED;
  } else if (self->state == NETWORK_CHANNEL_STATE_CLOSED) {
    COAP_UDP_IP_CHANNEL_ERR("Server connect handler: Channel is closed");
    code = COAP_RESPONSE_CODE_SERVICE_UNAVAILABLE;
  } else {
    // Do not update the state here (same as the RIOT channel): the connected state is only determined by our
    // client successfully connecting to the server of the other federate.
    // The peer (re)starts a session => its message IDs start from a new random value.
    self->last_rx_mid_valid = false;
    code = COAP_RESPONSE_CODE_CHANGED;
  }
  k_mutex_unlock(&coap_lock);
  return code;
}

static int _CoapUdpIpChannel_server_disconnect_handler(struct coap_resource* resource, struct coap_packet* request,
                                                       struct sockaddr* addr, socklen_t addr_len) {
  (void)resource;
  (void)request;
  (void)addr_len;
  COAP_UDP_IP_CHANNEL_DEBUG("Server disconnect handler");
  int code;

  k_mutex_lock(&coap_lock, K_FOREVER);
  CoapUdpIpChannel* self = _CoapUdpIpChannel_find_by_addr_locked(addr);
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server disconnect handler: Client has unknown IP address");
    code = COAP_RESPONSE_CODE_UNAUTHORIZED;
  } else {
    // It does not make sense to send data to a closed connection.
    if (self->state != NETWORK_CHANNEL_STATE_UNINITIALIZED) {
      _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CLOSED);
    }
    code = COAP_RESPONSE_CODE_CHANGED;
  }
  k_mutex_unlock(&coap_lock);
  return code;
}

static int _CoapUdpIpChannel_server_message_handler(struct coap_resource* resource, struct coap_packet* request,
                                                    struct sockaddr* addr, socklen_t addr_len) {
  (void)resource;
  (void)addr_len;
  COAP_UDP_IP_CHANNEL_DEBUG("Server message handler");
#if COAP_UDP_IP_CHANNEL_STATS
  int64_t handler_start = k_uptime_ticks();
#endif

  k_mutex_lock(&coap_lock, K_FOREVER);
  CoapUdpIpChannel* self = _CoapUdpIpChannel_find_by_addr_locked(addr);
  if (self == NULL) {
    k_mutex_unlock(&coap_lock);
    COAP_UDP_IP_CHANNEL_ERR("Server message handler: Client has unknown IP address");
    return COAP_RESPONSE_CODE_UNAUTHORIZED;
  }
  if (self->state == NETWORK_CHANNEL_STATE_CLOSED) {
    k_mutex_unlock(&coap_lock);
    COAP_UDP_IP_CHANNEL_WARN("Server message handler: Channel is closed");
    return COAP_RESPONSE_CODE_SERVICE_UNAVAILABLE;
  }

  // CoAP retransmits a CON request if our ACK got lost => do not deliver the message twice.
  // The sender has at most one CON message in flight per channel, so remembering the last ID is sufficient.
  // NON messages (see send_blocking) are never retransmitted and may arrive while a CON message is retransmitted,
  // so they neither are checked nor replace the remembered ID.
  uint16_t mid = coap_header_get_id(request);
  bool confirmable = coap_header_get_type(request) == COAP_TYPE_CON;
  if (confirmable && self->last_rx_mid_valid && self->last_rx_mid == mid) {
#if COAP_UDP_IP_CHANNEL_STATS
    self->stats.rx_dup++;
#endif
    k_mutex_unlock(&coap_lock);
    COAP_UDP_IP_CHANNEL_DEBUG("Server message handler: Duplicate message %u => only ACK", mid);
    return COAP_RESPONSE_CODE_CHANGED;
  }
  void (*receive_callback)(FederatedConnectionBundle*, const FederateMessage*) = self->receive_callback;
  FederatedConnectionBundle* conn = self->federated_connection;
  k_mutex_unlock(&coap_lock);

  // Only needed during this handler: the receive callback copies the message into the runtime's event queue.
  FederateMessage msg;
  uint16_t payload_len = 0;
  const uint8_t* payload = coap_packet_get_payload(request, &payload_len);
  if (payload == NULL || deserialize_from_protobuf(&msg, payload, payload_len) < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Server message handler: Could not decode message");
    return COAP_RESPONSE_CODE_BAD_REQUEST;
  }

  if (confirmable) {
    k_mutex_lock(&coap_lock, K_FOREVER);
    self->last_rx_mid = mid;
    self->last_rx_mid_valid = true;
    k_mutex_unlock(&coap_lock);
  }

  // Inform the runtime about the new message. Called without holding coap_lock.
  if (receive_callback != NULL) {
    receive_callback(conn, &msg);
  } else {
    COAP_UDP_IP_CHANNEL_WARN("Server message handler: No receive callback registered => dropping message");
  }

#if COAP_UDP_IP_CHANNEL_STATS
  k_mutex_lock(&coap_lock, K_FOREVER);
  if (confirmable) {
    self->stats.rx_con++;
  } else {
    self->stats.rx_non++;
  }
  _CoapUdpIpChannel_stats_record(self->stats.rx_handler_us, &self->stats.rx_handler_max_us,
                                 _CoapUdpIpChannel_us_since(handler_start));
  k_mutex_unlock(&coap_lock);
#endif
  return COAP_RESPONSE_CODE_CHANGED;
}

static uint16_t coap_server_port = COAP_UDP_IP_CHANNEL_PORT;
COAP_SERVICE_DEFINE(lf_coap_service, COAP_UDP_IP_CHANNEL_SERVER_HOST, &coap_server_port, 0);

static const char* const coap_connect_path[] = {COAP_PATH_CONNECT, NULL};
static const char* const coap_disconnect_path[] = {COAP_PATH_DISCONNECT, NULL};
static const char* const coap_message_path[] = {COAP_PATH_MESSAGE, NULL};

COAP_RESOURCE_DEFINE(lf_coap_connect_resource, lf_coap_service,
                     {
                         .path = coap_connect_path,
                         .post = _CoapUdpIpChannel_server_connect_handler,
                     });
COAP_RESOURCE_DEFINE(lf_coap_disconnect_resource, lf_coap_service,
                     {
                         .path = coap_disconnect_path,
                         .post = _CoapUdpIpChannel_server_disconnect_handler,
                     });
COAP_RESOURCE_DEFINE(lf_coap_message_resource, lf_coap_service,
                     {
                         .path = coap_message_path,
                         .post = _CoapUdpIpChannel_server_message_handler,
                     });

/* ------------------------------------------------------------------------------------------------- */
/* Global initialization                                                                              */
/* ------------------------------------------------------------------------------------------------- */

static bool _CoapUdpIpChannel_init_globals(int family) {
  coap_sock = zsock_socket(family, SOCK_DGRAM, IPPROTO_UDP);
  if (coap_sock < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to create client socket (%d)", errno);
    return false;
  }

  coap_wake_fd = zvfs_eventfd(0, ZVFS_EFD_NONBLOCK);
  if (coap_wake_fd < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to create eventfd (%d). Check CONFIG_ZVFS_EVENTFD_MAX", errno);
    return false;
  }

  int ret = coap_service_start(&lf_coap_service);
  if (ret < 0 && ret != -EALREADY) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to start CoAP server (%d)", ret);
    return false;
  }

  k_thread_create(&coap_client_thread, coap_client_stack, K_THREAD_STACK_SIZEOF(coap_client_stack),
                  _CoapUdpIpChannel_client_thread, NULL, NULL, NULL,
                  K_PRIO_PREEMPT(COAP_UDP_IP_CHANNEL_CLIENT_THREAD_PRIORITY), 0, K_NO_WAIT);
  k_thread_name_set(&coap_client_thread, "lf_coap_client");

  COAP_UDP_IP_CHANNEL_INFO("Initialized: send queue of %d messages per channel, max message size %d bytes, "
                           "channel size %d bytes",
                           COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE, COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE,
                           (int)sizeof(CoapUdpIpChannel));
  return true;
}

/* ------------------------------------------------------------------------------------------------- */
/* NetworkChannel interface (runtime thread)                                                          */
/* ------------------------------------------------------------------------------------------------- */

static lf_ret_t CoapUdpIpChannel_open_connection(NetworkChannel* untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Open connection");
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;

  if (!coap_globals_init_ok) {
    return LF_ERR;
  }

  // Server: nothing to do. Every federate is a CoAP server. The connection is considered established
  // once our client successfully connected to the server of the other federate.
  // Client: the client thread connects asynchronously.
  k_mutex_lock(&coap_lock, K_FOREVER);
  _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_OPEN);
  self->tx_deadline_ms = k_uptime_get_32(); // connect right away
  k_mutex_unlock(&coap_lock);

  _CoapUdpIpChannel_wake_client();
  return LF_OK;
}

/**
 * Must be called with coap_lock held. Wait until the queued messages are acknowledged (or dropped because the
 * connection was lost), at most until @p deadline (k_uptime_get()).
 */
static void _CoapUdpIpChannel_wait_queue_acked_locked(CoapUdpIpChannel* self, int64_t deadline) {
  while (self->state == NETWORK_CHANNEL_STATE_CONNECTED && self->queue_count > 0) {
    int64_t remaining = deadline - k_uptime_get();
    if (remaining <= 0) {
      COAP_UDP_IP_CHANNEL_WARN("%d queued messages not acknowledged in time", self->queue_count);
      return;
    }
    k_condvar_wait(&coap_cond, &coap_lock, K_MSEC(remaining));
  }
}

static void CoapUdpIpChannel_close_connection(NetworkChannel* untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Close connection");
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;
  int64_t deadline = k_uptime_get() + COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS;
  uint8_t buf[COAP_UDP_IP_CHANNEL_PACKET_SIZE];
  struct sockaddr remote;
  int len = 0;

  k_mutex_lock(&coap_lock, K_FOREVER);
  _CoapUdpIpChannel_wait_queue_acked_locked(self, deadline);

  // Immediately close the channel (drops what is still queued)
  bool was_open = self->state != NETWORK_CHANNEL_STATE_UNINITIALIZED && self->state != NETWORK_CHANNEL_STATE_CLOSED;
  _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CLOSED);

  // Inform the other federate that the channel is closed (best effort, replaces a /connect in flight)
  if (was_open && self->tx_kind != COAP_REQUEST_DISCONNECT) {
    _CoapUdpIpChannel_end_request_locked(self);
    len = _CoapUdpIpChannel_start_request_locked(self, COAP_REQUEST_DISCONNECT, buf);
    remote = self->remote;
  }
  k_mutex_unlock(&coap_lock);

  if (len > 0) {
    (void)_CoapUdpIpChannel_send_packet(buf, len, &remote);
    _CoapUdpIpChannel_wake_client(); // retransmissions
  }
}

/**
 * The timing messages of a clock-sync round are sent non-confirmable (NON). They carry the physical time at which
 * they were sent, so a delayed delivery is wrong rather than late: a retransmitted SyncResponse (or a message that
 * waited for the ACK of the previous one) arrives with an old timestamp and yields a wrong clock offset. A lost
 * message only loses the round (sequence numbers), and the next round starts one period later.
 * Priority messages build the clock-sync tree and stay confirmable.
 */
static bool _CoapUdpIpChannel_is_non_confirmable(const FederateMessage* message) {
  if (message->which_message != FederateMessage_clock_sync_msg_tag) {
    return false;
  }
  switch (message->message.clock_sync_msg.which_message) {
  case ClockSyncMessage_request_sync_tag:
  case ClockSyncMessage_sync_response_tag:
  case ClockSyncMessage_delay_request_tag:
  case ClockSyncMessage_delay_response_tag:
    return true;
  default:
    return false;
  }
}

/** Send a NON message right away: it bypasses the send queue. The server does not answer it. */
static lf_ret_t _CoapUdpIpChannel_send_non_confirmable(CoapUdpIpChannel* self, const FederateMessage* message) {
  uint8_t payload[COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE];
  int payload_len = serialize_to_protobuf(message, payload, sizeof(payload));
  if (payload_len < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Could not encode protobuf (larger than COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE = %d?)",
                            COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE);
    return LF_ERR;
  }

  k_mutex_lock(&coap_lock, K_FOREVER);
  bool connected = self->state == NETWORK_CHANNEL_STATE_CONNECTED;
  uint16_t mid = coap_next_id();
  struct sockaddr remote = self->remote;
#if COAP_UDP_IP_CHANNEL_STATS
  self->stats.sent_non += connected;
#endif
  k_mutex_unlock(&coap_lock);
  if (!connected) {
    return LF_ERR;
  }

  uint8_t buf[COAP_UDP_IP_CHANNEL_PACKET_SIZE];
  int len =
      _CoapUdpIpChannel_build_request(buf, COAP_TYPE_NON_CON, mid, COAP_PATH_MESSAGE, payload, (size_t)payload_len);
  if (len < 0 || _CoapUdpIpChannel_send_packet(buf, len, &remote) < 0) {
    return LF_ERR;
  }
  return LF_OK;
}

static lf_ret_t CoapUdpIpChannel_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  COAP_UDP_IP_CHANNEL_DEBUG("Send blocking");
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;

  if (_CoapUdpIpChannel_is_non_confirmable(message)) {
    return _CoapUdpIpChannel_send_non_confirmable(self, message);
  }

  uint8_t buf[COAP_UDP_IP_CHANNEL_PACKET_SIZE];
  struct sockaddr remote;
  int len = 0;
#if COAP_UDP_IP_CHANNEL_STATS
  int64_t wait_start = k_uptime_ticks();
#endif
  k_mutex_lock(&coap_lock, K_FOREVER);
  // Wait for a free slot in the send queue (woken up when a message is acknowledged or the queue is dropped).
  while (true) {
    if (self->state != NETWORK_CHANNEL_STATE_CONNECTED) {
      k_mutex_unlock(&coap_lock);
      return LF_ERR;
    }
    if (self->queue_count < COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE) {
      break;
    }
    k_condvar_wait(&coap_cond, &coap_lock, K_FOREVER);
  }
#if COAP_UDP_IP_CHANNEL_STATS
  _CoapUdpIpChannel_stats_record(self->stats.wait_us, &self->stats.wait_max_us, _CoapUdpIpChannel_us_since(wait_start));
#endif

  // Serialize directly into the queue slot. The client thread retransmits from there until the ACK arrives.
  CoapUdpIpChannelQueuedMessage* slot =
      &self->queue[(self->queue_head + self->queue_count) % COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE];
  int payload_len = serialize_to_protobuf(message, slot->data, sizeof(slot->data));
  if (payload_len < 0) {
    k_mutex_unlock(&coap_lock);
    COAP_UDP_IP_CHANNEL_ERR("Could not encode protobuf (larger than COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE = %d?)",
                            COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE);
    return LF_ERR;
  }
  slot->len = (uint16_t)payload_len;
  self->queue_count++;

  // The queue was empty => this message goes out right away. Otherwise the ACK of the previous one sends it.
  if (self->tx_kind == COAP_REQUEST_NONE) {
    len = _CoapUdpIpChannel_start_request_locked(self, COAP_REQUEST_MESSAGE, buf);
    remote = self->remote;
  }
  k_mutex_unlock(&coap_lock);

  if (len > 0) {
    // A failed send is like a lost packet: the client thread retransmits.
    (void)_CoapUdpIpChannel_send_packet(buf, len, &remote);
    _CoapUdpIpChannel_wake_client(); // new retransmission deadline
  }

#if COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK
  // Wait until the queue is acknowledged or dropped. This is bounded by the CoAP retransmission timeout.
  k_mutex_lock(&coap_lock, K_FOREVER);
  while (self->state == NETWORK_CHANNEL_STATE_CONNECTED && self->queue_count > 0) {
    k_condvar_wait(&coap_cond, &coap_lock, K_FOREVER);
  }
  bool success = self->state == NETWORK_CHANNEL_STATE_CONNECTED;
  k_mutex_unlock(&coap_lock);
  return success ? LF_OK : LF_ERR;
#else
  return LF_OK;
#endif
}

static void CoapUdpIpChannel_register_receive_callback(NetworkChannel* untyped_self,
                                                       void (*receive_callback)(FederatedConnectionBundle* conn,
                                                                                const FederateMessage* msg),
                                                       FederatedConnectionBundle* conn) {
  COAP_UDP_IP_CHANNEL_INFO("Register receive callback");
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;

  k_mutex_lock(&coap_lock, K_FOREVER);
  self->receive_callback = receive_callback;
  self->federated_connection = conn;
  k_mutex_unlock(&coap_lock);
}

static bool CoapUdpIpChannel_is_connected(NetworkChannel* untyped_self) {
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;
  k_mutex_lock(&coap_lock, K_FOREVER);
  bool connected = self->state == NETWORK_CHANNEL_STATE_CONNECTED;
  k_mutex_unlock(&coap_lock);
  return connected;
}

static void CoapUdpIpChannel_free(NetworkChannel* untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Free");
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;

  k_mutex_lock(&coap_lock, K_FOREVER);
  // The runtime frees the channels right after its last messages (e.g. FederateStopped, which tells the peer not to
  // wait for our inputs anymore) without closing them first. Deliver what is still queued (bounded).
  _CoapUdpIpChannel_wait_queue_acked_locked(self, k_uptime_get() + COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS);
  // The channel stays reachable through its bundle => reject requests of the peer and do not connect anymore.
  // A /disconnect in flight is still retransmitted by the client thread.
  _CoapUdpIpChannel_flush_queue_locked(self);
  if (self->tx_kind == COAP_REQUEST_CONNECT) {
    _CoapUdpIpChannel_end_request_locked(self);
  }
  self->state = NETWORK_CHANNEL_STATE_CLOSED;
  self->receive_callback = NULL;
#if COAP_UDP_IP_CHANNEL_STATS
  CoapUdpIpChannelStats st = self->stats;
#endif
  k_mutex_unlock(&coap_lock);

#if COAP_UDP_IP_CHANNEL_STATS
  // Buckets: <= t/3, t, 3t, 7t, 15t, more (t = ack timeout), i.e. about 1, 1, 2, 3, 4, 5 transmissions.
  char peer[NET_IPV6_ADDR_LEN] = "?";
  if (self->remote.sa_family == AF_INET) {
    zsock_inet_ntop(AF_INET, &((struct sockaddr_in*)&self->remote)->sin_addr, peer, sizeof(peer));
  } else {
    zsock_inet_ntop(AF_INET6, &((struct sockaddr_in6*)&self->remote)->sin6_addr, peer, sizeof(peer));
  }
  COAP_UDP_IP_CHANNEL_INFO("Stats %s: sent con=%u non=%u retransmissions=%u acked=%u failed=%u dropped=%u "
                           "late acks=%u",
                           peer, st.sent_con, st.sent_non, st.retransmissions, st.acked, st.failed, st.dropped,
                           st.late_acks);
  COAP_UDP_IP_CHANNEL_INFO("Stats %s: ack time %u/%u/%u/%u/%u/%u max %u us", peer, st.ack_us[0], st.ack_us[1],
                           st.ack_us[2], st.ack_us[3], st.ack_us[4], st.ack_us[5], st.ack_max_us);
  COAP_UDP_IP_CHANNEL_INFO("Stats %s: send wait %u/%u/%u/%u/%u/%u max %u us", peer, st.wait_us[0], st.wait_us[1],
                           st.wait_us[2], st.wait_us[3], st.wait_us[4], st.wait_us[5], st.wait_max_us);
  COAP_UDP_IP_CHANNEL_INFO("Stats %s: received con=%u non=%u duplicates=%u", peer, st.rx_con, st.rx_non, st.rx_dup);
  COAP_UDP_IP_CHANNEL_INFO("Stats %s: rx handler %u/%u/%u/%u/%u/%u max %u us", peer, st.rx_handler_us[0],
                           st.rx_handler_us[1], st.rx_handler_us[2], st.rx_handler_us[3], st.rx_handler_us[4],
                           st.rx_handler_us[5], st.rx_handler_max_us);
#endif
}

void CoapUdpIpChannel_ctor(CoapUdpIpChannel* self, const char* remote_address, int remote_protocol_family) {
  assert(self != NULL);
  assert(remote_address != NULL);

  // Super fields
  self->super.expected_connect_duration = COAP_UDP_IP_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.type = NETWORK_CHANNEL_TYPE_COAP_UDP_IP;
  self->super.mode = NETWORK_CHANNEL_MODE_ASYNC;
  self->super.is_connected = CoapUdpIpChannel_is_connected;
  self->super.open_connection = CoapUdpIpChannel_open_connection;
  self->super.close_connection = CoapUdpIpChannel_close_connection;
  self->super.send_blocking = CoapUdpIpChannel_send_blocking;
  self->super.register_receive_callback = CoapUdpIpChannel_register_receive_callback;
  self->super.free = CoapUdpIpChannel_free;

  // Concrete fields
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
  self->tx_kind = COAP_REQUEST_NONE;
  self->tx_transmissions = 0;
  self->tx_mid = 0;
  self->tx_timeout_ms = 0;
  self->tx_deadline_ms = 0;
  self->queue_head = 0;
  self->queue_count = 0;
  self->last_rx_mid_valid = false;
  self->last_rx_mid = 0;
#if COAP_UDP_IP_CHANNEL_STATS
  memset(&self->stats, 0, sizeof(self->stats));
#endif
  self->receive_callback = NULL;
  self->federated_connection = NULL;

  // Convert remote address
  memset(&self->remote, 0, sizeof(self->remote));
  int ret = 0;
#if defined(CONFIG_NET_IPV4)
  if (remote_protocol_family == AF_INET) {
    struct sockaddr_in* addr4 = net_sin(&self->remote);
    addr4->sin_family = AF_INET;
    addr4->sin_port = htons(COAP_UDP_IP_CHANNEL_PORT);
    ret = zsock_inet_pton(AF_INET, remote_address, &addr4->sin_addr);
  }
#endif
#if defined(CONFIG_NET_IPV6)
  if (remote_protocol_family == AF_INET6) {
    struct sockaddr_in6* addr6 = net_sin6(&self->remote);
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = htons(COAP_UDP_IP_CHANNEL_PORT);
    ret = zsock_inet_pton(AF_INET6, remote_address, &addr6->sin6_addr);
  }
#endif
  if (ret != 1) {
    COAP_UDP_IP_CHANNEL_ERR("Error parsing IP-Address %s (family %d)", remote_address, remote_protocol_family);
  }

  // Initialize the global CoAP server and client if not already done (after this channel's fields, because the
  // client thread may already look at it through its bundle).
  if (!coap_globals_initialized) {
    coap_globals_initialized = true;
    coap_globals_init_ok = _CoapUdpIpChannel_init_globals(remote_protocol_family);
  }
}
