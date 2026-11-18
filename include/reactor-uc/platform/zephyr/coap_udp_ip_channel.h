#ifndef REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H
#define REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H

/**
 * CoAP-over-UDP federated network channel built on Zephyr's native CoAP stack.
 *
 * Design (see src/platform/zephyr/coap_udp_ip_channel.c for details):
 *  - One global Zephyr coap_server service (UDP port COAP_UDP_IP_CHANNEL_PORT) with the resources
 *    /connect, /disconnect and /message. It runs in Zephyr's single CoAP server thread.
 *  - ONE Zephyr coap_client (and socket) shared by ALL channels, served by Zephyr's CoAP client thread.
 *    Its CONFIG_COAP_CLIENT_MAX_REQUESTS request entries form a window: up to COAP_UDP_IP_CHANNEL_WINDOW
 *    confirmable /message requests to different peers are in flight at the same time (at most one per
 *    channel); the remaining entries are reserved for /connect and /disconnect.
 *  - No thread, socket or message buffer per channel.
 *
 * The public interface is identical to the RIOT CoAP channel so that generated federate
 * code is platform agnostic.
 *
 * Required Kconfig (set in the application):
 *   CONFIG_COAP=y, CONFIG_COAP_SERVER=y, CONFIG_COAP_CLIENT=y,
 *   CONFIG_COAP_CLIENT_MESSAGE_SIZE   >= largest serialized message the application sends; this also sets
 *                                       COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE (e.g. 880 for any message,
 *                                       or e.g. 96 if all payloads are small),
 *   CONFIG_COAP_CLIENT_MESSAGE_HEADER_SIZE >= CoAP header + options (32 is enough),
 *   CONFIG_COAP_SERVER_MESSAGE_SIZE   >= COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE + CONFIG_COAP_CLIENT_MESSAGE_HEADER_SIZE,
 *   CONFIG_COAP_CLIENT_MAX_REQUESTS   =  COAP_UDP_IP_CHANNEL_WINDOW + COAP_UDP_IP_CHANNEL_CONTROL_REQUESTS
 *                                       (RAM: one request copy of MESSAGE_SIZE + HEADER_SIZE + ~230 bytes each),
 *   CONFIG_COAP_CLIENT_MAX_PATH_LENGTH>= 11 ("disconnect"), e.g. 16,
 *   CONFIG_COAP_CLIENT_MAX_INSTANCES  >= 1,
 *   CONFIG_COAP_SERVER_PENDING_ALLOCATOR_NONE=y (we only send piggybacked ACKs),
 *   CONFIG_COAP_SERVER_WELL_KNOWN_CORE=n,
 *   socket / net-context limits >= 3 (client socket, server socket, + 1).
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
 * Number of request entries of the shared CoAP client reserved for /connect and /disconnect. The other
 * CONFIG_COAP_CLIENT_MAX_REQUESTS - COAP_UDP_IP_CHANNEL_CONTROL_REQUESTS entries form the window for /message.
 */
#ifndef COAP_UDP_IP_CHANNEL_CONTROL_REQUESTS
#define COAP_UDP_IP_CHANNEL_CONTROL_REQUESTS 1
#endif

/** Maximum number of /message requests in flight at the same time (over all channels, at most one per channel). */
#define COAP_UDP_IP_CHANNEL_WINDOW (CONFIG_COAP_CLIENT_MAX_REQUESTS - COAP_UDP_IP_CHANNEL_CONTROL_REQUESTS)

/**
 * Size of the buffer a message is serialized into, i.e. the largest message the channel can send. The buffer lives
 * on the stack of the thread calling send_blocking (the runtime thread), so it adds to that thread's stack usage.
 * Defaults to the largest possible FederateMessage (+ 2 bytes length prefix), limited by
 * CONFIG_COAP_CLIENT_MESSAGE_SIZE. An application that only sends small payloads lowers
 * CONFIG_COAP_CLIENT_MESSAGE_SIZE (and CONFIG_COAP_SERVER_MESSAGE_SIZE) to save RAM in every request entry;
 * a larger message then fails to serialize with an error instead of being sent.
 */
#ifndef COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE
#if CONFIG_COAP_CLIENT_MESSAGE_SIZE < FederateMessage_size + 2
#define COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE CONFIG_COAP_CLIENT_MESSAGE_SIZE
#else
#define COAP_UDP_IP_CHANNEL_MAX_MESSAGE_SIZE (FederateMessage_size + 2)
#endif
#endif

/**
 * 1: send_blocking waits until the message is acknowledged (or failed) by the remote federate.
 *    Then only one message is in flight at a time and the window has no effect.
 * 0: send_blocking returns as soon as the message is handed to the CoAP client. At most one message per
 *    channel and COAP_UDP_IP_CHANNEL_WINDOW messages in total are in flight; the next send on the same
 *    channel waits for the previous ACK. A failed message is reported by the channel switching to
 *    NETWORK_CHANNEL_STATE_LOST_CONNECTION.
 */
#ifndef COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK
#define COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK 0
#endif

/**
 * Transmission parameters for /connect and /disconnect. They use the same ACK timeout as /message (Zephyr's
 * default parameters, see coap_set_transmission_parameters; define COAP_UDP_IP_CHANNEL_CONNECT_ACK_TIMEOUT_MS to
 * override it), but only COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT retransmissions: a failed connect is retried
 * by the channel anyway, so a peer that is not up yet should free its request entry quickly.
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

/** Upper bound close_connection waits for an in-flight message and for a free request entry. */
#ifndef COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS
#define COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS 5000
#endif

typedef struct CoapUdpIpChannel CoapUdpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct CoapUdpIpChannel {
  NetworkChannel super;

  NetworkChannelState state; // protected by the global channel lock
  struct sockaddr remote;    // remote federate (IP + COAP_UDP_IP_CHANNEL_PORT)

  bool connect_pending; // a /connect request of this channel is in flight
  bool msg_in_flight;   // a /message of this channel is not yet acknowledged
  bool msg_succeeded;   // result of the last completed /message

  bool last_rx_mid_valid; // duplicate detection of incoming /message requests
  uint16_t last_rx_mid;

  FederatedConnectionBundle* federated_connection;
  void (*receive_callback)(FederatedConnectionBundle* conn, const FederateMessage* message);
};

void CoapUdpIpChannel_ctor(CoapUdpIpChannel* self, const char* remote_address, int remote_protocol_family);

#endif // REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H
