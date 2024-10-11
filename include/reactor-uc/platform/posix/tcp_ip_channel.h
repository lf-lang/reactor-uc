#ifndef REACTOR_UC_TCP_IP_CHANNEL_H
#define REACTOR_UC_TCP_IP_CHANNEL_H
#include <nanopb/pb.h>
#include <pthread.h>
#include <sys/select.h>

#include "proto/message.pb.h"
#include "reactor-uc/error.h"
#include "reactor-uc/network_channel.h"

#define TCP_IP_CHANNEL_BUFFERSIZE 1024
#define TCP_IP_CHANNEL_NUM_RETRIES 255;

typedef struct TcpIpChannel TcpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct TcpIpChannel {
  NetworkChannel super;

  int fd;
  int client;

  const char *host;
  unsigned short port;
  int protocol_family;

  TaggedMessage output;
  unsigned char write_buffer[TCP_IP_CHANNEL_BUFFERSIZE];
  unsigned char read_buffer[TCP_IP_CHANNEL_BUFFERSIZE];
  unsigned int read_index;

  fd_set set;
  bool server;
  bool blocking;
  bool terminate;

  // required for callbacks
  pthread_t receive_thread;
  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, TaggedMessage *message);
};

void TcpIpChannel_ctor(TcpIpChannel *self, const char *host, unsigned short port, int protocol_family);

void TcpIpChannel_free(NetworkChannel *self);

#endif
