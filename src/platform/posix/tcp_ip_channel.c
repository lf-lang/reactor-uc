#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/encoding.h"
#include "reactor-uc/logging.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proto/message.pb.h"

#ifdef MIN
#undef MIN
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

lf_ret_t TcpIpChannel_bind(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  // turn human-readable address into something the os can work with
  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    return LF_INVALID_VALUE;
  }

  // bind the socket to that address
  int ret = bind(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (ret < 0) {
    return LF_NETWORK_SETUP_FAILED;
  }

  // start listening
  if (listen(self->fd, 1) < 0) {
    return LF_NETWORK_SETUP_FAILED;
  }

  return LF_OK;
}

lf_ret_t TcpIpChannel_connect(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  self->server = false;

  struct sockaddr_in serv_addr;

  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    return LF_INVALID_VALUE;
  }

  int ret = connect(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (ret < 0) {
    return LF_COULD_NOT_CONNECT;
  }

  return LF_OK;
}

bool TcpIpChannel_accept(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  int new_socket;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);

  new_socket = accept(self->fd, (struct sockaddr *)&address, &addrlen);
  if (new_socket >= 0) {
    self->client = new_socket;
    FD_SET(new_socket, &self->set);

    return true;
  }
  return false;
}

lf_ret_t TcpIpChannel_send(NetworkChannel *untyped_self, TaggedMessage *message) {
  LF_DEBUG(NET, "TcpIpChannel sending message");
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  int socket;

  // based if this super is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->client;
  } else {
    socket = self->fd;
  }

  // serializing protobuf into buffer
  int message_size = encode_protobuf(message, self->write_buffer, TCP_IP_CHANNEL_BUFFERSIZE);

  if (message_size < 0) {
    LF_ERR(NET, "Could not encode protobuf");
    return LF_ERR;
  }

  // sending serialized data
  ssize_t bytes_written = 0;
  int timeout = TCP_IP_CHANNEL_NUM_RETRIES;

  while (bytes_written < message_size && timeout > 0) {
    LF_DEBUG(NET, "Sending %d bytes", message_size - bytes_written);
    int bytes_send = send(socket, self->write_buffer + bytes_written, message_size - bytes_written, 0);
    LF_DEBUG(NET, "%d bytes sent", bytes_send);

    if (bytes_send < 0) {
      LF_ERR(NET, "write failed errno=%d", errno);
      return LF_ERR;
    }

    bytes_written += bytes_send;
    timeout--;
  }

  // checking if the whole message was transmitted or timeout was received
  if (timeout == 0 || bytes_written < message_size) {
    LF_ERR(NET, "Timeout on sending TCpIpChannel message");
    return LF_ERR;
  }

  return LF_OK;
}

TaggedMessage *TcpIpChannel_receive(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  int socket;

  // based if this super is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->client;
  } else {
    socket = self->fd;
  }

  // calculating the maximum amount of bytes we can read
  int bytes_available = TCP_IP_CHANNEL_BUFFERSIZE - self->read_index;
  int bytes_left = 0;
  bool read_more = true;

  while (read_more) {

    // reading from socket
    int bytes_read = recv(socket, self->read_buffer + self->read_index, bytes_available, 0);

    if (bytes_read < 0) {
      LF_ERR(NET, "Error recv from socket %d", errno);
      continue;
    }

    self->read_index += bytes_read;
    bytes_left = decode_protobuf(&self->output, self->read_buffer, self->read_index);
    if (bytes_left < 0) {
      read_more = true;
    } else {
      read_more = false;
    }
  }

  memcpy(self->read_buffer, self->read_buffer + (self->read_index - bytes_left), bytes_left);
  self->read_index = bytes_left;

  return &self->output;
}

void TcpIpChannel_close(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  if (self->server) {
    close(self->client);
  }
  close(self->fd);
}

void *TcpIpChannel_receive_thread(void *untyped_self) {
  LF_INFO(NET, "Starting TCP/IP receive thread");
  TcpIpChannel *self = untyped_self;

  // set terminate to false so the loop runs
  self->terminate = false;

  while (!self->terminate) {
    TaggedMessage *msg = self->super.receive(untyped_self);

    if (msg) {
      self->receive_callback(self->federated_connection, msg);
    }
  }

  return NULL;
}

void TcpIpChannel_register_callback(NetworkChannel *untyped_self,
                                    void (*receive_callback)(FederatedConnectionBundle *conn, TaggedMessage *msg),
                                    FederatedConnectionBundle *conn) {
  int res;
  LF_INFO(NET, "TCP/IP registering callback thread");
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
  memset(&self->receive_thread_stack, 0, TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE);
  if (pthread_attr_init(&self->receive_thread_attr) < 0) {
    throw("pthread_attr_init failed");
  }
  if (pthread_attr_setstack(&self->receive_thread_attr, &self->receive_thread_stack,
                            TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE - TCP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE) < 0) {
    throw("pthread_attr_setstack failed");
  }
  res = pthread_create(&self->receive_thread, &self->receive_thread_attr, TcpIpChannel_receive_thread, self);
  if (res < 0) {
    throw("pthread_create failed");
  }
}

void TcpIpChannel_ctor(TcpIpChannel *self, const char *host, unsigned short port, int protocol_family) {
  FD_ZERO(&self->set);

  if ((self->fd = socket(protocol_family, SOCK_STREAM, 0)) < 0) {
    exit(1);
  }

  self->server = true;
  self->terminate = true;
  self->protocol_family = protocol_family;
  self->host = host;
  self->port = port;
  self->read_index = 0;
  self->client = 0;

  self->super.accept = TcpIpChannel_accept;
  self->super.bind = TcpIpChannel_bind;
  self->super.connect = TcpIpChannel_connect;
  self->super.close = TcpIpChannel_close;
  self->super.receive = TcpIpChannel_receive;
  self->super.send = TcpIpChannel_send;
  self->super.register_callback = TcpIpChannel_register_callback;
  self->super.free = TcpIpChannel_free;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
}

void TcpIpChannel_free(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  self->terminate = true;

  if (self->receive_thread != 0) {
    pthread_cancel(self->receive_thread);
    pthread_join(self->receive_thread, NULL);
  }
  self->super.close((NetworkChannel *)self);
}