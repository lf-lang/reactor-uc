#ifndef REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H
#define REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H

/**
 * CoAP-over-UDP federated network channel built on Zephyr's CoAP packet library.
 *
 * Design (see src/platform/zephyr/coap_udp_ip_channel.c for details):
 *  - Server: one global Zephyr coap_server service (UDP port COAP_UDP_IP_CHANNEL_PORT) with the resources
 *    /connect, /disconnect and /message. It runs in Zephyr's CoAP server thread.
 *  - Client: our own, built with the CoAP packet API on ONE UDP socket shared by all channels. One client thread
 *    (COAP_UDP_IP_CHANNEL_CLIENT_STACK_SIZE) receives the ACKs and retransmits with Zephyr's CoAP transmission
 *    parameters (see coap_set_transmission_parameters). Zephyr's coap_client is not used.
 *  - Every channel has a send queue of COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE serialized messages. The oldest one is
 *    in flight as a confirmable request, so at most one message per channel is unacknowledged (keeps the order and
 *    makes duplicate detection by message ID sufficient). send_blocking only waits if the queue is full.
 *
 * The public interface is identical to the RIOT CoAP channel so that generated federate
 * code is platform agnostic.
 *
 * Required Kconfig (set in the application):
 *   CONFIG_COAP=y, CONFIG_COAP_SERVER=y,
 *   CONFIG_COAP_SERVER_MESSAGE_SIZE   >= COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE + COAP_UDP_IP_CHANNEL_HEADER_SIZE,
 *   CONFIG_COAP_SERVER_PENDING_ALLOCATOR_NONE=y (we only send piggybacked ACKs),
 *   CONFIG_COAP_SERVER_WELL_KNOWN_CORE=n,
 *   CONFIG_ZVFS_EVENTFD_MAX           >= 2 (one for the CoAP server, one to wake up the client thread),
 *   socket / net-context limits >= 2 (client socket and server socket) + what the application uses.
 */

#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

/** Expected time until a connection is established after calling open_connection. */
#ifndef COAP_UDP_IP_CHANNEL_EXPECTED_CONNECT_DURATION
#define COAP_UDP_IP_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(10)
#endif

/** UDP port of the CoAP server on every federate. */
#ifndef COAP_UDP_IP_CHANNEL_PORT
#define COAP_UDP_IP_CHANNEL_PORT 5683
#endif

/**
 * Local address the CoAP server binds to. Defaults to IPv4 any if IPv4 is enabled, otherwise
 * Zephyr's default (IPv6 any). Define as "::" to force IPv6.
 */
#ifndef COAP_UDP_IP_CHANNEL_SERVER_HOST
#if defined(CONFIG_NET_IPV4)
#define COAP_UDP_IP_CHANNEL_SERVER_HOST "0.0.0.0"
#else
#define COAP_UDP_IP_CHANNEL_SERVER_HOST NULL
#endif
#endif

/**
 * Largest serialized message the channel can send and receive. Every queue slot of every channel holds one, so this is
 * the main RAM knob together with COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE. Defaults to the largest possible
 * FederateMessage (+ 2 bytes length prefix). An application that only sends small payloads lowers it (and
 * CONFIG_COAP_SERVER_MESSAGE_SIZE); a larger message then fails to serialize with an error instead of being sent.
 */
#ifndef COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE
#define COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE (FederateMessage_size + 2)
#endif

/** CoAP header and options of a request (4 bytes header, Uri-Path "disconnect", Content-Format, payload marker). */
#define COAP_UDP_IP_CHANNEL_HEADER_SIZE 20

/**
 * Number of messages per channel that send_blocking can hand over without waiting for an ACK. The oldest message is
 * in flight, the others wait for its ACK. Costs COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE + 2 bytes per slot and channel.
 * With 1, send_blocking returns right after the first transmission, but the next message to the same peer waits for
 * the ACK of the previous one (including its retransmissions).
 */
#ifndef COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE
#define COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE 1
#endif

#if COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE < 1 || COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE > 255
#error "COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE must be in 1..255"
#endif

/** Maximum number of /connect requests in flight at the same time (over all channels). */
#ifndef COAP_UDP_IP_CHANNEL_CONNECT_WINDOW
#define COAP_UDP_IP_CHANNEL_CONNECT_WINDOW 16
#endif

/** Stack size and priority of the client thread (ACKs, retransmissions, connect requests). */
#ifndef COAP_UDP_IP_CHANNEL_CLIENT_STACK_SIZE
#define COAP_UDP_IP_CHANNEL_CLIENT_STACK_SIZE 1536
#endif
#ifndef COAP_UDP_IP_CHANNEL_CLIENT_THREAD_PRIORITY
#define COAP_UDP_IP_CHANNEL_CLIENT_THREAD_PRIORITY 10
#endif

/**
 * 1: send_blocking waits until all messages of the channel are acknowledged (or failed) by the remote federate.
 * 0: send_blocking returns as soon as the message is in the send queue (see COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE).
 *    A failed message is reported by the channel switching to NETWORK_CHANNEL_STATE_LOST_CONNECTION; the messages
 *    still in the queue are dropped.
 */
#ifndef COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK
#define COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK 0
#endif

/**
 * Transmission parameters for /connect and /disconnect. They use the same ACK timeout as /message (Zephyr's
 * default parameters, see coap_set_transmission_parameters; define COAP_UDP_IP_CHANNEL_CONNECT_ACK_TIMEOUT_MS to
 * override it), but only COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT retransmissions: a failed connect is retried
 * by the channel anyway, so a peer that is not up yet should free its place in the connect window quickly.
 */
#ifndef COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT
#define COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT 1
#endif

/**
 * Delay before the next round of connect attempts after a connect failed. Kept short, so that peers that boot late
 * are retried soon (all peers that are still not connected are retried round-robin in every round).
 */
#ifndef COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS
#define COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS 100
#endif

/** Upper bound close_connection and free wait until the send queue is acknowledged. */
#ifndef COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS
#define COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS 5000
#endif

/**
 * 1: every channel counts its messages and prints the counters when it is freed (diagnostics, see
 * CoapUdpIpChannelStats). Times are sorted into COAP_UDP_IP_CHANNEL_STATS_BUCKETS buckets whose upper bounds are
 * the times of the CoAP retransmissions with the default random factor 1 (ack timeout t: t/3, t, 3t, 7t, 15t), so the
 * bucket of an ACK time is about the number of transmissions the message needed.
 */
#ifndef COAP_UDP_IP_CHANNEL_STATS
#define COAP_UDP_IP_CHANNEL_STATS 0
#endif

#define COAP_UDP_IP_CHANNEL_STATS_BUCKETS 6

#if COAP_UDP_IP_CHANNEL_STATS
typedef struct {
  int64_t sent_ticks;       // first transmission of the message in flight
  uint32_t sent_con;        // confirmable /message requests (first transmissions)
  uint32_t sent_non;        // non-confirmable /message requests (clock-sync timing messages)
  uint32_t retransmissions; // retransmissions of confirmable /message requests
  uint32_t acked;           // confirmable requests that were acknowledged
  uint32_t failed;          // confirmable requests that failed (no ACK after all retransmissions, reset, error code)
  uint32_t dropped;         // queued messages dropped because the connection was lost or closed
  uint32_t late_acks;       // ACKs that match no request in flight (e.g. the ACK of an earlier transmission)
  uint32_t ack_us[COAP_UDP_IP_CHANNEL_STATS_BUCKETS]; // time from the first transmission to the ACK
  uint32_t ack_max_us;
  uint32_t wait_us[COAP_UDP_IP_CHANNEL_STATS_BUCKETS]; // time send_blocking waited for a free queue slot
  uint32_t wait_max_us;
  uint32_t rx_con; // confirmable /message requests received and delivered
  uint32_t rx_non; // non-confirmable /message requests received and delivered
  uint32_t rx_dup; // retransmissions of an already delivered request (our ACK was lost or late)
  uint32_t rx_handler_us[COAP_UDP_IP_CHANNEL_STATS_BUCKETS]; // time of the /message handler (delays our ACK)
  uint32_t rx_handler_max_us;
} CoapUdpIpChannelStats;
#endif

typedef struct CoapUdpIpChannel CoapUdpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

/** A serialized FederateMessage in the send queue of a channel. */
typedef struct {
  uint16_t len;
  uint8_t data[COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE];
} CoapUdpIpChannelQueuedMessage;

/** All fields are protected by the global channel lock. */
struct CoapUdpIpChannel {
  NetworkChannel super;

  NetworkChannelState state;
  struct sockaddr remote; // remote federate (IP + COAP_UDP_IP_CHANNEL_PORT)

  // The confirmable request in flight (at most one per channel): /connect, /disconnect or the head of the queue.
  uint8_t tx_kind;          // CoapUdpIpChannelRequestKind (none if nothing is in flight)
  uint8_t tx_transmissions; // transmissions so far
  uint16_t tx_mid;          // CoAP message ID of the request in flight
  uint32_t tx_timeout_ms;   // current ACK timeout (doubles with every retransmission)
  uint32_t tx_deadline_ms;  // k_uptime_get_32() of the next retransmission, or of the next connect attempt

  uint8_t queue_head;     // index of the oldest message
  uint8_t queue_count;    // number of queued messages, incl. the one in flight
  bool last_rx_mid_valid; // duplicate detection of incoming /message requests
  uint16_t last_rx_mid;
  CoapUdpIpChannelQueuedMessage queue[COAP_UDP_IP_CHANNEL_SEND_QUEUE_SIZE];

#if COAP_UDP_IP_CHANNEL_STATS
  CoapUdpIpChannelStats stats; // protected by the global channel lock
#endif

  FederatedConnectionBundle* federated_connection;
  void (*receive_callback)(FederatedConnectionBundle* conn, const FederateMessage* message);
};

void CoapUdpIpChannel_ctor(CoapUdpIpChannel* self, const char* remote_address, int remote_protocol_family);

#endif // REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H
