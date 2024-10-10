#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include "reactor-uc/encoding.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>

#include "proto/message.pb.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

lf_ret_t TcpIpBundle_bind(TcpIpBundle *self) {
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

lf_ret_t TcpIpBundle_connect(TcpIpBundle *self) {
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

bool TcpIpBundle_accept(TcpIpBundle *self) {
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

lf_ret_t TcpIpBundle_send(TcpIpBundle *self, TaggedMessage *message) {
  int socket;

  // based if this bundle is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->client;
  } else {
    socket = self->fd;
  }

  // serializing protobuf into buffer
  int message_size = encode_protobuf(message, self->write_buffer, TCP_IP_BUNDLE_BUFFERSIZE);

  if (message_size < 0) {
    return LF_ERR;
  }

  // sending serialized data
  ssize_t bytes_written = 0;
  int timeout = TCP_IP_NUM_RETRIES;

  while (bytes_written < message_size && timeout > 0) {
    int bytes_send = write(socket, self->write_buffer + bytes_written, message_size - bytes_written);

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

TaggedMessage *TcpIpBundle_receive(TcpIpBundle *self) {
  int bytes_available;
  int socket;

  // based if this bundle is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->client;
  } else {
    socket = self->fd;
  }

  if (self->blocking) {
    ioctl(socket, FIONREAD, &bytes_available);
    bytes_available = MIN(8, bytes_available);
  } else {
    // peek into file descriptor to figure how many bytes are available.
    ioctl(socket, FIONREAD, &bytes_available);
  }

  if (bytes_available == 0) {
    return NULL;
  }

  // calculating the maximum amount of bytes we can read
  if (bytes_available + self->read_index >= TCP_IP_BUNDLE_BUFFERSIZE) {
    bytes_available = TCP_IP_BUNDLE_BUFFERSIZE - self->read_index;
  }

  // reading from socket
  int bytes_read = read(socket, self->read_buffer + self->read_index, bytes_available);

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

void TcpIpBundle_close(TcpIpBundle *self) {
  if (self->server) {
    close(self->client);
  }
  close(self->fd);
}

void TcpIpBundle_change_block_state(TcpIpBundle *self, bool blocking) {
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

void *TcpIpBundle_receive_thread(void *untyped_self) {
  TcpIpBundle *self = untyped_self;

  // turning on blocking receive on this socket
  self->change_block_state(self, true);

  // set terminate to false so the loop runs
  self->terminate = false;

  while (!self->terminate) {
    TaggedMessage *msg = self->receive(self);

    if (msg) {
      self->receive_callback(self->federated_connection, msg);
    }
  }

  return NULL;
}

void TcpIpBundle_register_callback(TcpIpBundle *self,
                                   void (*receive_callback)(FederatedConnectionBundle *conn, TaggedMessage *msg),
                                   FederatedConnectionBundle *conn) {
  self->receive_callback = receive_callback;
  self->federated_connection = conn;
  self->receive_thread = pthread_create(&self->receive_thread, NULL, TcpIpBundle_receive_thread, self);
}

void TcpIpBundle_ctor(TcpIpBundle *self, const char *host, unsigned short port, int protocol_family) {
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

  self->accept = TcpIpBundle_accept;
  self->bind = TcpIpBundle_bind;
  self->connect = TcpIpBundle_connect;
  self->close = TcpIpBundle_close;
  self->receive = TcpIpBundle_receive;
  self->send = TcpIpBundle_send;
  self->change_block_state = TcpIpBundle_change_block_state;
  self->register_callback = TcpIpBundle_register_callback;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
}

void TcpIpBundle_free(TcpIpBundle *self) {
  printf("Freeing TcpIpBundle\n");
  self->terminate = true;
  pthread_join(self->receive_thread, NULL);
  self->close(self);
}