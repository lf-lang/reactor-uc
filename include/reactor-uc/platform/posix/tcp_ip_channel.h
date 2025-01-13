#ifndef REACTOR_UC_TCP_IP_CHANNEL_H
#define REACTOR_UC_TCP_IP_CHANNEL_H
#include <nanopb/pb.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "proto/message.pb.h"
#include "reactor-uc/error.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#define TCP_IP_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(10)
#define TCP_IP_CHANNEL_WORKER_THREAD_MAIN_LOOP_SLEEP MSEC(100)
#define TCP_IP_CHANNEL_BUFFERSIZE 1024
#define TCP_IP_CHANNEL_NUM_RETRIES 255
#define TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE 2048
#define TCP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE 128

typedef struct TcpIpChannel TcpIpChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

struct TcpIpChannel {
  NetworkChannel super;

  int fd;
  int client;
  int send_failed_event_fds[2]; // These file descriptors are used to signal the recv select to stop blocking
  int terminate_event_fds;
  NetworkChannelState state;
  pthread_mutex_t mutex;

  const char *host;
  unsigned short port;
  int protocol_family;

  FederateMessage output;
  unsigned char write_buffer[TCP_IP_CHANNEL_BUFFERSIZE];
  unsigned char read_buffer[TCP_IP_CHANNEL_BUFFERSIZE];
  unsigned int read_index;

  fd_set set;
  bool is_server;
  bool terminate;

  // required for callbacks
  pthread_t worker_thread;
  pthread_attr_t worker_thread_attr;
  char worker_thread_stack[TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE];

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

void TcpIpChannel_ctor(TcpIpChannel *self, Environment *env, const char *host, unsigned short port, int protocol_family,
                       bool is_server);

#endif
