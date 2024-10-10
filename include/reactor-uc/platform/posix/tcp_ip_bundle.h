#ifndef REACTOR_UC_TCP_IP_BUNDLE_H
#define REACTOR_UC_TCP_IP_BUNDLE_H
#include <nanopb/pb.h>
#include <pthread.h>
#include <sys/select.h>

#include "proto/message.pb.h"
#include "reactor-uc/error.h"
#include "reactor-uc/network_bundles.h"

#define TCP_IP_BUNDLE_BUFFERSIZE 1024
#define TCP_IP_NUM_RETRIES 255;

typedef struct TcpIpBundle TcpIpBundle;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct TcpIpBundle {
  NetworkBundle bundle;

  int fd;
  int client;

  const char *host;
  unsigned short port;
  int protocol_family;

  TaggedMessage output;
  unsigned char write_buffer[TCP_IP_BUNDLE_BUFFERSIZE];
  unsigned char read_buffer[TCP_IP_BUNDLE_BUFFERSIZE];
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

void TcpIpBundle_ctor(TcpIpBundle *self, const char *host, unsigned short port, int protocol_family);

void TcpIpBundle_free(TcpIpBundle *self);

#endif
