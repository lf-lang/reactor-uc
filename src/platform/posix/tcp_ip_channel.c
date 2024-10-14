#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/encoding.h"

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

#include "proto/message.pb.h"

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
    printf("ret = %d, errno=%d\n", ret, errno);

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
  printf("ret=%d errno=%d\n", ret, errno);
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
  printf("accept ret=%d, errno=%d\n", new_socket, errno);
  return false;
}

lf_ret_t TcpIpChannel_send(NetworkChannel *untyped_self, TaggedMessage *message) {
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
    return LF_ERR;
  }

  // sending serialized data
  ssize_t bytes_written = 0;
  int timeout = TCP_IP_CHANNEL_NUM_RETRIES;

  while (bytes_written < message_size && timeout > 0) {
    int bytes_send = send(socket, self->write_buffer + bytes_written, message_size - bytes_written, 0);

    if (bytes_send < 0) {
      return LF_ERR;
    }

    bytes_written += bytes_send;
    timeout--;
  }

  // checking if the whole message was transmitted or timeout was received
  if (timeout == 0 || bytes_written < message_size) {
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

  // configure select to check if data is available (non blocking)
  fd_set read_fds;
  struct timeval timeout;
  FD_ZERO(&read_fds);
  FD_SET(socket, &read_fds);
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  // check if data is available
  int select_result = select(socket + 1, &read_fds, NULL, NULL, &timeout);
  if (select_result <= 0) {
    return NULL;
  }

  // calculating the maximum amount of bytes we can read
  int bytes_available = TCP_IP_CHANNEL_BUFFERSIZE - self->read_index;

  // reading from socket
  int bytes_read = recv(socket, self->read_buffer + self->read_index, bytes_available, 0);

  if (bytes_read < 0) {
    printf("error during reading errno: %i\n", errno);
    return NULL;
  }

  self->read_index += bytes_read;

  int bytes_left = decode_protobuf(&self->output, self->read_buffer, self->read_index);

  if (bytes_left < 0) {
    return NULL;
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

void TcpIpChannel_change_block_state(NetworkChannel *untyped_self, bool blocking) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  self->blocking = blocking;

  int fd_socket_config = fcntl(self->fd, F_GETFL);

  if (blocking) {
    fcntl(self->fd, F_SETFL, fd_socket_config | (~O_NONBLOCK));

    if (self->server) {
      fcntl(self->client, F_SETFL, fd_socket_config | (~O_NONBLOCK));
    }
  } else {
    // configure the socket to be non-blocking

    fcntl(self->fd, F_SETFL, fd_socket_config | O_NONBLOCK);

    if (self->server) {
      fcntl(self->client, F_SETFL, fd_socket_config | O_NONBLOCK);
    }
  }
}

void *TcpIpChannel_receive_thread(void *untyped_self) {
  TcpIpChannel *self = untyped_self;

  // turning on blocking receive on this socket
  self->super.change_block_state(untyped_self, true);

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
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
  self->receive_thread = pthread_create(&self->receive_thread, NULL, TcpIpChannel_receive_thread, self);
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
  self->super.change_block_state = TcpIpChannel_change_block_state;
  self->super.register_callback = TcpIpChannel_register_callback;
  self->super.free = TcpIpChannel_free;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
}

void TcpIpChannel_free(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  self->terminate = true;

  if (self->receive_thread != 0) {
    pthread_join(self->receive_thread, NULL);
  }
  self->super.close((NetworkChannel *)self);
}