
#include <nanopb/pb.h>
#include <sys/select.h>

#include "reactor-uc/generated/message.pb.h"

typedef struct TcpIpBundle TcpIpBundle;

struct TcpIpBundle {
  int fd;

  int clients[30];
  unsigned int clients_size;
  unsigned int clients_capacity;
  unsigned int clients_index;

  const char* host;
  unsigned short port;
  int protocol_family;

  unsigned char write_buffer[1024];
  unsigned char read_buffer[1024];
  PortMessage output;
  unsigned int read_index;

  fd_set set;
  bool server;

  void (*bind)(TcpIpBundle* self);
  void (*connect)(TcpIpBundle* self);
  bool (*accept)(TcpIpBundle* self);
  void (*close)(TcpIpBundle* self);

  void (*send)(TcpIpBundle* self, PortMessage* message);
  PortMessage* (*receive)();
};

void TcpIpBundle_Server_Ctor(TcpIpBundle* self, const char* host, unsigned short port, int protocol_family);
