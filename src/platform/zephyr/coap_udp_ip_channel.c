/**
 * CoAP-over-UDP federated network channel built on Zephyr's native CoAP stack.
 *
 * Port of src/platform/riot/coap_udp_ip_channel.c. The protocol on the wire is identical:
 * every federate runs a CoAP server with the resources /connect, /disconnect and /message and
 * talks to its peers with confirmable POST requests. Peers are identified by their IP address.
 *
 * Threads / execution contexts:
 *  - Runtime thread:          open_connection, send_blocking, close_connection, ...
 *  - Zephyr CoAP server thread: server resource handlers (incl. the receive callback)
 *  - Zephyr CoAP client thread: request callbacks (ACK received / timeout)
 *  - System workqueue:        connect_work, issues (re)connect requests
 *
 * Memory:
 *  - Per channel: only CoapUdpIpChannel (~60 bytes, no buffers).
 *  - Global: COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE coap_clients (+ sockets), one payload buffer per data
 *    client, one FederateMessage for decoding incoming messages.
 *
 * Locking: `coap_lock` protects the channel list, all channel fields that change at runtime and the
 * client pool. It is never held while calling into coap_client (which may need the client thread to
 * finish a callback that itself takes `coap_lock`) or while calling the receive callback.
 */
#include "reactor-uc/platform/zephyr/coap_udp_ip_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/serialization.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/coap_client.h>
#include <zephyr/net/coap_service.h>
#include <zephyr/version.h>

// Zephyr v4.3 changed the coap_client API: `path`/`options` of struct coap_client_request became inline
// arrays and the response callback receives a `struct coap_client_response_data*`.
#if ZEPHYR_VERSION_CODE >= ZEPHYR_VERSION(4, 2, 99)
#define COAP_CLIENT_API_V2 1
#else
#define COAP_CLIENT_API_V2 0
#endif

#define COAP_UDP_IP_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)

#define COAP_PATH_CONNECT "connect"
#define COAP_PATH_DISCONNECT "disconnect"
#define COAP_PATH_MESSAGE "message"

// How often coap_client_req is retried if the client is still finishing a previous request.
// Zephyr invokes the response callback *before* marking the request as finished, so a client that was
// just released in its callback may reject a new request for a short moment.
#define COAP_CLIENT_REQ_BUSY_RETRIES 100

#define COAP_IS_SUCCESS(code) ((code) >= 0 && ((code) >> 5) == 2)

/* ------------------------------------------------------------------------------------------------- */
/* Global state                                                                                       */
/* ------------------------------------------------------------------------------------------------- */

typedef enum {
  COAP_SLOT_REQUEST_CONNECT,
  COAP_SLOT_REQUEST_DISCONNECT,
  COAP_SLOT_REQUEST_MESSAGE,
} CoapSlotRequest;

typedef struct {
  struct coap_client client;
  int sock;
  bool busy;
  CoapSlotRequest request;
  CoapUdpIpChannel* owner;
} CoapClientSlot;

// Slots [0, DATA_CLIENTS) are reserved for /message, the others for /connect and /disconnect.
static CoapClientSlot coap_pool[COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE];

// Zephyr's coap_client rebuilds the request (incl. payload) on every retransmission,
// so the payload must stay valid until the request completed.
static uint8_t coap_tx_buf[COAP_UDP_IP_CHANNEL_DATA_CLIENTS][FederateMessage_size];

// Decoded incoming message. Only used from the CoAP server thread.
static FederateMessage coap_rx_msg;

static CoapUdpIpChannel* coap_channels = NULL;
static CoapUdpIpChannel* coap_connect_cursor = NULL; // round-robin start for connect attempts
static bool coap_globals_initialized = false;
static bool coap_globals_init_ok = false;

static K_MUTEX_DEFINE(coap_lock);
static K_CONDVAR_DEFINE(coap_cond); // signalled whenever a slot is released or a request completes

static void _CoapUdpIpChannel_connect_work_handler(struct k_work* work);
static K_WORK_DELAYABLE_DEFINE(coap_connect_work, _CoapUdpIpChannel_connect_work_handler);

static struct coap_transmission_parameters coap_connect_params = {
    .ack_timeout = COAP_UDP_IP_CHANNEL_CONNECT_ACK_TIMEOUT_MS,
    .coap_backoff_percent = 200,
    .max_retransmission = COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT,
};

/* ------------------------------------------------------------------------------------------------- */
/* Helpers                                                                                            */
/* ------------------------------------------------------------------------------------------------- */

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

/** Must be called with coap_lock held. */
static CoapUdpIpChannel* _CoapUdpIpChannel_find_by_addr_locked(const struct sockaddr* addr) {
  for (CoapUdpIpChannel* ch = coap_channels; ch != NULL; ch = ch->next) {
    if (_CoapUdpIpChannel_addr_equal(&ch->remote, addr)) {
      return ch;
    }
  }
  return NULL;
}

/**
 * Update the state of a channel. Must be called with coap_lock held.
 * Informs the runtime if the channel became connected or disconnected.
 */
static void _CoapUdpIpChannel_set_state_locked(CoapUdpIpChannel* self, NetworkChannelState new_state) {
  NetworkChannelState old_state = self->state;
  if (old_state == new_state) {
    return;
  }
  COAP_UDP_IP_CHANNEL_DEBUG("Update state: %s => %s", NetworkChannel_state_to_string(old_state),
                            NetworkChannel_state_to_string(new_state));
  self->state = new_state;

  if ((old_state == NETWORK_CHANNEL_STATE_CONNECTED) != (new_state == NETWORK_CHANNEL_STATE_CONNECTED)) {
    _lf_environment->platform->notify(_lf_environment->platform);
  }
}

static bool _CoapUdpIpChannel_needs_connect_locked(const CoapUdpIpChannel* self) {
  if (self->connect_pending) {
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

/** Must be called with coap_lock held. Returns a free slot in [first, last) and marks it busy, or NULL. */
static CoapClientSlot* _CoapUdpIpChannel_acquire_slot_locked(size_t first, size_t last, CoapUdpIpChannel* owner,
                                                             CoapSlotRequest request) {
  for (size_t i = first; i < last; i++) {
    if (!coap_pool[i].busy) {
      coap_pool[i].busy = true;
      coap_pool[i].owner = owner;
      coap_pool[i].request = request;
      return &coap_pool[i];
    }
  }
  return NULL;
}

/** Must be called with coap_lock held. */
static void _CoapUdpIpChannel_release_slot_locked(CoapClientSlot* slot) {
  slot->busy = false;
  slot->owner = NULL;
  k_condvar_broadcast(&coap_cond);
}

static void _CoapUdpIpChannel_kick_connect_work(k_timeout_t delay) {
  if (K_TIMEOUT_EQ(delay, K_NO_WAIT)) {
    k_work_reschedule(&coap_connect_work, K_NO_WAIT);
  } else {
    // Do not postpone an already scheduled (earlier) run.
    k_work_schedule(&coap_connect_work, delay);
  }
}

static void _CoapUdpIpChannel_handle_response(int16_t result_code, bool last_block, void* user_data);

#if COAP_CLIENT_API_V2
static void _CoapUdpIpChannel_response_cb(const struct coap_client_response_data* data, void* user_data) {
  _CoapUdpIpChannel_handle_response(data->result_code, data->last_block, user_data);
}
#else
static void _CoapUdpIpChannel_response_cb(int16_t result_code, size_t offset, const uint8_t* payload, size_t len,
                                          bool last_block, void* user_data) {
  (void)offset;
  (void)payload;
  (void)len;
  _CoapUdpIpChannel_handle_response(result_code, last_block, user_data);
}
#endif

/**
 * Issue a confirmable POST on a pool client. Must be called WITHOUT coap_lock held.
 */
static int _CoapUdpIpChannel_send_request(CoapClientSlot* slot, const struct sockaddr* remote, const char* path,
                                          uint8_t* payload, size_t payload_len,
                                          struct coap_transmission_parameters* params) {
  struct coap_client_request req;
  memset(&req, 0, sizeof(req));
  req.method = COAP_METHOD_POST;
  req.confirmable = true;
#if COAP_CLIENT_API_V2
  strncpy(req.path, path, sizeof(req.path) - 1); // path is copied into the request
#else
  req.path = path; // string literal, stays valid
#endif
  req.fmt = COAP_CONTENT_FORMAT_APP_OCTET_STREAM;
  req.payload = payload;
  req.len = payload_len;
  req.cb = _CoapUdpIpChannel_response_cb;
  req.num_options = 0;
  req.user_data = slot;

  int ret = -EAGAIN;
  for (int i = 0; i < COAP_CLIENT_REQ_BUSY_RETRIES; i++) {
    ret = coap_client_req(&slot->client, slot->sock, remote, &req, params);
    if (ret != -EAGAIN && ret != -EALREADY) {
      break;
    }
    k_msleep(1);
  }

  if (ret < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to send CoAP request /%s (%d)", path, ret);
  }
  return ret;
}

/* ------------------------------------------------------------------------------------------------- */
/* Client side: response callback (Zephyr CoAP client thread)                                         */
/* ------------------------------------------------------------------------------------------------- */

static void _CoapUdpIpChannel_handle_response(int16_t result_code, bool last_block, void* user_data) {
  if (!last_block) {
    return; // Not expected: we never use block-wise transfers.
  }

  CoapClientSlot* slot = (CoapClientSlot*)user_data;
  bool success = COAP_IS_SUCCESS(result_code);
  k_timeout_t connect_delay = K_FOREVER;

  k_mutex_lock(&coap_lock, K_FOREVER);
  CoapUdpIpChannel* self = slot->owner;
  CoapSlotRequest request = slot->request;
  _CoapUdpIpChannel_release_slot_locked(slot);

  if (self != NULL) {
    switch (request) {
    case COAP_SLOT_REQUEST_CONNECT:
      COAP_UDP_IP_CHANNEL_DEBUG("Connect response: %d", result_code);
      self->connect_pending = false;
      if (self->state == NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS) {
        if (success) {
          _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTED);
        } else {
          COAP_UDP_IP_CHANNEL_WARN("Connect failed (%d) => try again", result_code);
          _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
        }
      }
      // Free control slot => let other channels connect. Retry failed ones after a delay.
      connect_delay = success ? K_NO_WAIT : K_MSEC(COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS);
      break;

    case COAP_SLOT_REQUEST_MESSAGE:
      COAP_UDP_IP_CHANNEL_DEBUG("Message response: %d", result_code);
      self->msg_in_flight = false;
      self->msg_succeeded = success;
      if (!success) {
        COAP_UDP_IP_CHANNEL_ERR("Message was not acknowledged (%d) => lost connection", result_code);
        if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
          _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
          connect_delay = K_NO_WAIT;
        }
      }
      break;

    case COAP_SLOT_REQUEST_DISCONNECT:
      COAP_UDP_IP_CHANNEL_DEBUG("Disconnect response: %d", result_code);
      connect_delay = K_NO_WAIT; // a control slot became free
      break;
    }
  } else {
    // The channel was freed while its request was in flight. The slot is free again, so let channels that are
    // waiting for a control slot connect now (they are only retried when a response releases a slot).
    connect_delay = K_NO_WAIT;
  }
  k_mutex_unlock(&coap_lock);

  if (!K_TIMEOUT_EQ(connect_delay, K_FOREVER)) {
    _CoapUdpIpChannel_kick_connect_work(connect_delay);
  }
}

/* ------------------------------------------------------------------------------------------------- */
/* Client side: connection management (system workqueue)                                             */
/* ------------------------------------------------------------------------------------------------- */

/** Must be called with coap_lock held. Round-robin search for the next channel that needs to connect. */
static CoapUdpIpChannel* _CoapUdpIpChannel_next_to_connect_locked(void) {
  CoapUdpIpChannel* start = coap_connect_cursor != NULL ? coap_connect_cursor : coap_channels;
  CoapUdpIpChannel* ch = start;
  if (ch == NULL) {
    return NULL;
  }
  do {
    if (_CoapUdpIpChannel_needs_connect_locked(ch)) {
      coap_connect_cursor = ch->next;
      return ch;
    }
    ch = ch->next != NULL ? ch->next : coap_channels;
  } while (ch != start);
  return NULL;
}

static void _CoapUdpIpChannel_connect_work_handler(struct k_work* work) {
  (void)work;

  while (true) {
    k_mutex_lock(&coap_lock, K_FOREVER);
    CoapUdpIpChannel* self = _CoapUdpIpChannel_next_to_connect_locked();
    if (self == NULL) {
      k_mutex_unlock(&coap_lock);
      return;
    }
    CoapClientSlot* slot = _CoapUdpIpChannel_acquire_slot_locked(
        COAP_UDP_IP_CHANNEL_DATA_CLIENTS, COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE, self, COAP_SLOT_REQUEST_CONNECT);
    if (slot == NULL) {
      // All control clients busy. The next response callback kicks this work again.
      coap_connect_cursor = self;
      k_mutex_unlock(&coap_lock);
      return;
    }
    self->connect_pending = true;
    _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS);
    k_mutex_unlock(&coap_lock);

    COAP_UDP_IP_CHANNEL_DEBUG("Sending connect request");
    int ret = _CoapUdpIpChannel_send_request(slot, &self->remote, COAP_PATH_CONNECT, NULL, 0, &coap_connect_params);
    if (ret < 0) {
      k_mutex_lock(&coap_lock, K_FOREVER);
      _CoapUdpIpChannel_release_slot_locked(slot);
      self->connect_pending = false;
      if (self->state == NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS) {
        _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
      }
      k_mutex_unlock(&coap_lock);
      _CoapUdpIpChannel_kick_connect_work(K_MSEC(COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS));
      return;
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
  // The sender has at most one message in flight per channel, so remembering the last ID is sufficient.
  uint16_t mid = coap_header_get_id(request);
  if (self->last_rx_mid_valid && self->last_rx_mid == mid) {
    k_mutex_unlock(&coap_lock);
    COAP_UDP_IP_CHANNEL_DEBUG("Server message handler: Duplicate message %u => only ACK", mid);
    return COAP_RESPONSE_CODE_CHANGED;
  }
  void (*receive_callback)(FederatedConnectionBundle*, const FederateMessage*) = self->receive_callback;
  FederatedConnectionBundle* conn = self->federated_connection;
  k_mutex_unlock(&coap_lock);

  uint16_t payload_len = 0;
  const uint8_t* payload = coap_packet_get_payload(request, &payload_len);
  if (payload == NULL || deserialize_from_protobuf(&coap_rx_msg, payload, payload_len) < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Server message handler: Could not decode message");
    return COAP_RESPONSE_CODE_BAD_REQUEST;
  }

  k_mutex_lock(&coap_lock, K_FOREVER);
  self->last_rx_mid = mid;
  self->last_rx_mid_valid = true;
  k_mutex_unlock(&coap_lock);

  // Inform the runtime about the new message. Called without holding coap_lock.
  if (receive_callback != NULL) {
    receive_callback(conn, &coap_rx_msg);
  } else {
    COAP_UDP_IP_CHANNEL_WARN("Server message handler: No receive callback registered => dropping message");
  }

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
  for (size_t i = 0; i < COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE; i++) {
    CoapClientSlot* slot = &coap_pool[i];
    slot->busy = false;
    slot->owner = NULL;

    slot->sock = zsock_socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (slot->sock < 0) {
      COAP_UDP_IP_CHANNEL_ERR("Failed to create client socket %d (%d)", (int)i, errno);
      return false;
    }

    int ret = coap_client_init(&slot->client, NULL);
    if (ret < 0) {
      COAP_UDP_IP_CHANNEL_ERR("Failed to init CoAP client %d (%d). Check CONFIG_COAP_CLIENT_MAX_INSTANCES", (int)i,
                              ret);
      return false;
    }
  }

  int ret = coap_service_start(&lf_coap_service);
  if (ret < 0 && ret != -EALREADY) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to start CoAP server (%d)", ret);
    return false;
  }

  COAP_UDP_IP_CHANNEL_INFO("Initialized: %d shared CoAP clients (%d for data), channel size %d bytes",
                           COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE, COAP_UDP_IP_CHANNEL_DATA_CLIENTS,
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
  // Client: connect asynchronously from the system workqueue.
  k_mutex_lock(&coap_lock, K_FOREVER);
  _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_OPEN);
  k_mutex_unlock(&coap_lock);

  _CoapUdpIpChannel_kick_connect_work(K_NO_WAIT);
  return LF_OK;
}

static void CoapUdpIpChannel_close_connection(NetworkChannel* untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Close connection");
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;
  int64_t deadline = k_uptime_get() + COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS;

  k_mutex_lock(&coap_lock, K_FOREVER);

  // Let the last message be delivered before closing (relevant if send_blocking does not wait for ACKs).
  while (self->msg_in_flight) {
    int64_t remaining = deadline - k_uptime_get();
    if (remaining <= 0 || k_condvar_wait(&coap_cond, &coap_lock, K_MSEC(remaining)) != 0) {
      COAP_UDP_IP_CHANNEL_WARN("Close connection: last message not acknowledged in time");
      break;
    }
  }

  // Immediately close the channel
  bool was_open = self->state != NETWORK_CHANNEL_STATE_UNINITIALIZED && self->state != NETWORK_CHANNEL_STATE_CLOSED;
  _CoapUdpIpChannel_set_state_locked(self, NETWORK_CHANNEL_STATE_CLOSED);

  // Inform the other federate that the channel is closed (best effort)
  CoapClientSlot* slot = NULL;
  while (was_open) {
    slot = _CoapUdpIpChannel_acquire_slot_locked(COAP_UDP_IP_CHANNEL_DATA_CLIENTS, COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE,
                                                 self, COAP_SLOT_REQUEST_DISCONNECT);
    int64_t remaining = deadline - k_uptime_get();
    if (slot != NULL || remaining <= 0 || k_condvar_wait(&coap_cond, &coap_lock, K_MSEC(remaining)) != 0) {
      break;
    }
  }
  k_mutex_unlock(&coap_lock);

  if (slot != NULL) {
    if (_CoapUdpIpChannel_send_request(slot, &self->remote, COAP_PATH_DISCONNECT, NULL, 0, &coap_connect_params) < 0) {
      k_mutex_lock(&coap_lock, K_FOREVER);
      _CoapUdpIpChannel_release_slot_locked(slot);
      k_mutex_unlock(&coap_lock);
    }
  } else if (was_open) {
    COAP_UDP_IP_CHANNEL_WARN("Close connection: no free CoAP client to send disconnect");
  }
}

static lf_ret_t CoapUdpIpChannel_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  COAP_UDP_IP_CHANNEL_DEBUG("Send blocking");
  CoapUdpIpChannel* self = (CoapUdpIpChannel*)untyped_self;
  CoapClientSlot* slot = NULL;

  k_mutex_lock(&coap_lock, K_FOREVER);
  // Wait until the previous message of this channel is acknowledged and a data client is free.
  while (true) {
    if (self->state != NETWORK_CHANNEL_STATE_CONNECTED) {
      k_mutex_unlock(&coap_lock);
      return LF_ERR;
    }
    if (!self->msg_in_flight) {
      slot =
          _CoapUdpIpChannel_acquire_slot_locked(0, COAP_UDP_IP_CHANNEL_DATA_CLIENTS, self, COAP_SLOT_REQUEST_MESSAGE);
      if (slot != NULL) {
        break;
      }
    }
    k_condvar_wait(&coap_cond, &coap_lock, K_FOREVER);
  }
  self->msg_in_flight = true;
  self->msg_succeeded = false;
  k_mutex_unlock(&coap_lock);

  // Serialize message into the payload buffer of this data client
  uint8_t* buf = coap_tx_buf[slot - coap_pool];
  int payload_len = serialize_to_protobuf(message, buf, sizeof(coap_tx_buf[0]));
  int ret = -EINVAL;
  if (payload_len < 0) {
    COAP_UDP_IP_CHANNEL_ERR("Could not encode protobuf");
  } else {
    ret = _CoapUdpIpChannel_send_request(slot, &self->remote, COAP_PATH_MESSAGE, buf, (size_t)payload_len, NULL);
  }

  if (ret < 0) {
    k_mutex_lock(&coap_lock, K_FOREVER);
    _CoapUdpIpChannel_release_slot_locked(slot);
    self->msg_in_flight = false;
    k_mutex_unlock(&coap_lock);
    return LF_ERR;
  }

#if COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK
  // Wait until the response callback confirms the ACK or the request timed out.
  // This is bounded by the CoAP retransmission timeout.
  k_mutex_lock(&coap_lock, K_FOREVER);
  while (self->msg_in_flight) {
    k_condvar_wait(&coap_cond, &coap_lock, K_FOREVER);
  }
  bool success = self->msg_succeeded && self->state == NETWORK_CHANNEL_STATE_CONNECTED;
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
  // Detach from in-flight requests so their callbacks do not touch this channel anymore
  for (size_t i = 0; i < COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE; i++) {
    if (coap_pool[i].owner == self) {
      coap_pool[i].owner = NULL;
    }
  }
  // Unlink from the channel list
  for (CoapUdpIpChannel** it = &coap_channels; *it != NULL; it = &(*it)->next) {
    if (*it == self) {
      *it = self->next;
      break;
    }
  }
  if (coap_connect_cursor == self) {
    coap_connect_cursor = NULL;
  }
  self->next = NULL;
  k_mutex_unlock(&coap_lock);
}

void CoapUdpIpChannel_ctor(CoapUdpIpChannel* self, const char* remote_address, int remote_protocol_family) {
  assert(self != NULL);
  assert(remote_address != NULL);

  // Initialize global CoAP server and client pool if not already done
  if (!coap_globals_initialized) {
    coap_globals_initialized = true;
    coap_globals_init_ok = _CoapUdpIpChannel_init_globals(remote_protocol_family);
  }

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
  self->connect_pending = false;
  self->msg_in_flight = false;
  self->msg_succeeded = false;
  self->last_rx_mid_valid = false;
  self->last_rx_mid = 0;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->next = NULL;

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

  // Register channel globally (lookup by remote address in the server handlers)
  k_mutex_lock(&coap_lock, K_FOREVER);
  self->next = coap_channels;
  coap_channels = self;
  k_mutex_unlock(&coap_lock);
}
