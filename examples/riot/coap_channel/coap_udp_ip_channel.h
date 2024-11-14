#ifndef REACTOR_UC_COAP_UDP_IP_CHANNEL_H
#define REACTOR_UC_COAP_UDP_IP_CHANNEL_H
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"
#include "net/sock/udp.h"

// #define COAP_UDP_IP_CHANNEL_BUFFERSIZE 1024
// #define COAP_UDP_IP_CHANNEL_NUM_RETRIES 255;
// #define COAP_UDP_IP_CHANNEL_RECV_THREAD_STACK_SIZE 2048
// #define COAP_UDP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE 128

typedef struct CoapUdpIpChannel CoapUdpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct CoapUdpIpChannel {
  NetworkChannel super;

  sock_udp_ep_t remote;

  NetworkChannelState state;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

void CoapUdpIpChannel_ctor(CoapUdpIpChannel *self, Environment *env, const char *remote_host);

#endif
