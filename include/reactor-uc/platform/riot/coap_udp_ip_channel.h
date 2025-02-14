#ifndef REACTOR_UC_COAP_UDP_IP_CHANNEL_H
#define REACTOR_UC_COAP_UDP_IP_CHANNEL_H
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"
#include "net/sock/udp.h"
#include "mutex.h"

#define COAP_UDP_IP_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(10);
#define COAP_UDP_IP_CHANNEL_BUFFERSIZE 1024
// #define COAP_UDP_IP_CHANNEL_NUM_RETRIES 255;
// #define COAP_UDP_IP_CHANNEL_RECV_THREAD_STACK_SIZE 2048
// #define COAP_UDP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE 128

typedef struct CoapUdpIpChannel CoapUdpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct CoapUdpIpChannel {
  NetworkChannel super;

  NetworkChannelState state;
  mutex_t state_mutex;

  sock_udp_ep_t remote;

  bool send_ack_received;
  bool was_ever_connected;
  FederateMessage output;
  uint8_t write_buffer[COAP_UDP_IP_CHANNEL_BUFFERSIZE];

  EncryptionLayer *encryption_layer;
  void (*receive_callback)(EncryptionLayer *encryption_layer, const char *message, ssize_t message_size);
};

void CoapUdpIpChannel_ctor(CoapUdpIpChannel *self, const char *remote_address, int remote_protocol_family);

#endif
