#ifndef REACTOR_UC_COAP_CHANNEL_H
#define REACTOR_UC_COAP_CHANNEL_H
#include "reactor-uc/network_channel.h"
#include "net/sock/udp.h"

// #define COAP_CHANNEL_BUFFERSIZE 1024
// #define COAP_CHANNEL_NUM_RETRIES 255;
// #define COAP_CHANNEL_RECV_THREAD_STACK_SIZE 2048
// #define COAP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE 128

typedef enum {
  SERVER_COAP_CHANNEL,
  CLIENT_COAP_CHANNEL,
} CoapChannelType;

typedef struct CoapChannel CoapChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct CoapChannel {
  NetworkChannel super;

  const char *local_host;
  unsigned short local_port;
  const char *remote_host;
  unsigned short remote_port;

  bool connected_to_server;
  bool connected_to_client;

  sock_udp_ep_t remote;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

void CoapChannel_ctor(CoapChannel *self, const char *local_host, unsigned short local_port, const char *remote_host,
                      unsigned short remote_port);

#endif
