#ifndef REACTOR_UC_COAP_UDP_IP_CHANNEL_H
#define REACTOR_UC_COAP_UDP_IP_CHANNEL_H
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"
#include <pthread.h>
#include <coap3/coap.h>

#define COAP_UDP_IP_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(10);
#define COAP_UDP_IP_CHANNEL_BUFFERSIZE 1024
#define COAP_UDP_IP_CHANNEL_RECV_THREAD_STACK_SIZE 2048

typedef struct CoapUdpIpChannel CoapUdpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

typedef enum {
  COAP_REQUEST_TYPE_NONE,
  COAP_REQUEST_TYPE_CONNECT,
  COAP_REQUEST_TYPE_MESSAGE,
  COAP_REQUEST_TYPE_DISCONNECT
} coap_request_type_t;

typedef struct CoapUdpIpChannel {
  NetworkChannel super;

  // Remote address etc.
  coap_session_t *session;

  // Threading and synchronization
  pthread_mutex_t state_mutex;
  pthread_cond_t state_cond;
  pthread_mutex_t send_mutex;
  pthread_cond_t send_cond;

  NetworkChannelState state;

  FederateMessage output;

  // Handle message callbacks
  coap_request_type_t last_request_type;
  coap_mid_t last_request_mid;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
} CoapUdpIpChannel;

/**
 * @brief Constructor for the CoapUdpIpChannel.
 *
 * Initializes a CoapUdpIpChannel instance with the specified remote host and protocol family.
 *
 * @param self Pointer to the CoapUdpIpChannel instance.
 * @param remote_host The remote host address, hostname or domain E.g. 127.0.0.1, [::1] or hostname.local.
 * @param remote_protocol_family The protocol family (e.g., AF_INET for IPv4 and AF_INET6 for IPv6).
 */
void CoapUdpIpChannel_ctor(CoapUdpIpChannel *self, const char *remote_host, int remote_protocol_family);

#endif
