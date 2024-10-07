#include "reactor-uc/error.h"
#include <nanopb/pb.h>
#include <sys/select.h>

#include "proto/message.pb.h"

#define TCP_BUNDLE_BUFFERSIZE 1024

typedef struct TcpIpBundle TcpIpBundle;

struct TcpIpBundle {
  int fd;
  int client;

  const char *host;
  unsigned short port;
  int protocol_family;

  unsigned char write_buffer[TCP_BUNDLE_BUFFERSIZE];
  unsigned char read_buffer[TCP_BUNDLE_BUFFERSIZE];
  PortMessage output;
  unsigned int read_index;

  fd_set set;
  bool server;
  bool blocking;

  lf_ret_t (*bind)(TcpIpBundle *self);
  lf_ret_t (*connect)(TcpIpBundle *self);
  bool (*accept)(TcpIpBundle *self);
  void (*close)(TcpIpBundle *self);
  void (*change_block_state)(TcpIpBundle *self, bool blocking);

  lf_ret_t (*send)(TcpIpBundle *self, PortMessage *message);
  PortMessage *(*receive)();
};

void TcpIpBundle_ctor(TcpIpBundle *self, const char *host, unsigned short port, int protocol_family);
