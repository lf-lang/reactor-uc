#ifndef REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H
#define REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H

/**
 * CoAP-over-UDP federated network channel built on Zephyr's native CoAP stack.
 *
 * Design (see src/platform/zephyr/coap_udp_ip_channel.c for details):
 *  - One global Zephyr coap_server service (UDP port COAP_UDP_IP_CHANNEL_PORT) with the resources
 *    /connect, /disconnect and /message. It runs in Zephyr's single CoAP server thread.
 *  - A small, fixed pool of Zephyr coap_clients shared by ALL channels. A client is lent to a channel
 *    for exactly one confirmable request. All clients are served by Zephyr's single CoAP client thread.
 *  - No thread, socket or message buffer per channel.
 *
 * The public interface is identical to the RIOT CoAP channel so that generated federate
 * code is platform agnostic.
 *
 * Required Kconfig (set in the application):
 *   CONFIG_COAP=y, CONFIG_COAP_SERVER=y, CONFIG_COAP_CLIENT=y,
 *   CONFIG_COAP_CLIENT_MESSAGE_SIZE   >= FederateMessage_size (+ some slack, e.g. 880),
 *   CONFIG_COAP_SERVER_MESSAGE_SIZE   >= FederateMessage_size + CoAP header (e.g. 920),
 *   CONFIG_COAP_CLIENT_MAX_REQUESTS=1,
 *   CONFIG_COAP_CLIENT_MAX_INSTANCES  >= COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE,
 *   CONFIG_COAP_SERVER_PENDING_ALLOCATOR_NONE=y (we only send piggybacked ACKs),
 *   CONFIG_COAP_SERVER_WELL_KNOWN_CORE=n,
 *   socket / net-context limits >= COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE + 2.
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
 * Total number of shared Zephyr coap_clients (each has its own socket and send/receive buffers of
 * CONFIG_COAP_CLIENT_MESSAGE_SIZE + header). Must be >= COAP_UDP_IP_CHANNEL_DATA_CLIENTS + 1.
 */
#ifndef COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE
#define COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE 2
#endif

/**
 * Number of pool clients reserved for /message requests. The remaining clients are used for
 * /connect and /disconnect. Each data client additionally owns a payload buffer of FederateMessage_size
 * because Zephyr's coap_client re-reads the payload on every retransmission.
 */
#ifndef COAP_UDP_IP_CHANNEL_DATA_CLIENTS
#define COAP_UDP_IP_CHANNEL_DATA_CLIENTS 1
#endif

/**
 * 1: send_blocking waits until the message is acknowledged (or failed) by the remote federate.
 * 0: send_blocking returns as soon as the message is handed to the CoAP client. At most one message per
 *    channel is in flight; the next send on the same channel waits for the previous ACK. A failed message
 *    is reported by the channel switching to NETWORK_CHANNEL_STATE_LOST_CONNECTION.
 */
#ifndef COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK
#define COAP_UDP_IP_CHANNEL_SEND_WAIT_FOR_ACK 1
#endif

/** Transmission parameters for /connect and /disconnect (short, so a dead peer frees a slot quickly). */
#ifndef COAP_UDP_IP_CHANNEL_CONNECT_ACK_TIMEOUT_MS
#define COAP_UDP_IP_CHANNEL_CONNECT_ACK_TIMEOUT_MS 500
#endif
#ifndef COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT
#define COAP_UDP_IP_CHANNEL_CONNECT_MAX_RETRANSMIT 1
#endif

/** Delay before retrying connections after a failed connect attempt. */
#ifndef COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS
#define COAP_UDP_IP_CHANNEL_RECONNECT_DELAY_MS 500
#endif

/** Upper bound close_connection waits for an in-flight message and for a free control client. */
#ifndef COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS
#define COAP_UDP_IP_CHANNEL_CLOSE_TIMEOUT_MS 5000
#endif

#if COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE < COAP_UDP_IP_CHANNEL_DATA_CLIENTS + 1
#error "COAP_UDP_IP_CHANNEL_CLIENT_POOL_SIZE must be at least COAP_UDP_IP_CHANNEL_DATA_CLIENTS + 1"
#endif

typedef struct CoapUdpIpChannel CoapUdpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct CoapUdpIpChannel {
  NetworkChannel super;

  NetworkChannelState state; // protected by the global channel lock
  struct sockaddr remote;    // remote federate (IP + COAP_UDP_IP_CHANNEL_PORT)

  bool connect_pending; // a pool client currently performs /connect for this channel
  bool msg_in_flight;   // a /message of this channel is not yet acknowledged
  bool msg_succeeded;   // result of the last completed /message

  bool last_rx_mid_valid; // duplicate detection of incoming /message requests
  uint16_t last_rx_mid;

  FederatedConnectionBundle* federated_connection;
  void (*receive_callback)(FederatedConnectionBundle* conn, const FederateMessage* message);

  CoapUdpIpChannel* next; // global channel list
};

void CoapUdpIpChannel_ctor(CoapUdpIpChannel* self, const char* remote_address, int remote_protocol_family);

#endif // REACTOR_UC_ZEPHYR_COAP_UDP_IP_CHANNEL_H
