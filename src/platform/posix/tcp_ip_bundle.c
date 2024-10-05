#include "reactor-uc/platform/posix/tcp_ip_bundle.h"

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>

#include "reactor-uc/generated/message.pb.h"

BundleResponse TcpIpBundle_bind(TcpIpBundle *self) {
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  // turn human-readable address into something the os can work with
  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    return INVALID_ADDRESS;
  }

  // bind the socket to that address
  if (bind(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    return BIND_FAILED;
  }

  // start listening
  if (listen(self->fd, 1) < 0) {
    return LISTENING_FAILED;
  }
  return SUCCESS;
}

BundleResponse TcpIpBundle_connect(TcpIpBundle *self) {
  self->server = false;

  struct sockaddr_in serv_addr;

  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    return INVALID_ADDRESS;
  }

  if (connect(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    return CONNECT_FAILED;
  }

  return SUCCESS;
}

bool TcpIpBundle_accept(TcpIpBundle *self) {
  int new_socket;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);

  if ((new_socket = accept(self->fd, (struct sockaddr *)&address, &addrlen)) > 0) {
    self->client = new_socket;
    FD_SET(new_socket, &self->set);

    return true;
  }
  return false;
}

BundleResponse TcpIpBundle_send(TcpIpBundle *self, PortMessage *message) {
  int socket;

  // based if this bundle is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->client;
  } else {
    socket = self->fd;
  }

  // turing write buffer into pb_ostream buffer
  pb_ostream_t stream = pb_ostream_from_buffer(self->write_buffer, 1024);

  // serializing protobuf into buffer
  int status = pb_encode(&stream, PortMessage_fields, message);

  if (status < 0) {
    return ENCODING_ERROR;
  }

  // sending serialized data to client
  size_t bytes_written = write(socket, self->write_buffer, stream.bytes_written);

  // checking if the whole message was transmitted
  if (bytes_written < stream.bytes_written) {
    return INCOMPLETE_MESSAGE_ERROR;
  }

  return SUCCESS;
}

PortMessage *TcpIpBundle_receive(TcpIpBundle *self) {
  int bytes_available;
  int socket;

  // based if this bundle is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->client;
  } else {
    socket = self->fd;
  }

  // peek into file descriptor to figure how many bytes are available.
  ioctl(socket, FIONREAD, &bytes_available);

  if (bytes_available == 0) {
    return NULL;
  }

  // calculating the maximum amount of bytes we can read
  if (bytes_available + self->read_index >= 1024) {
    bytes_available = 1024 - self->read_index;
  }

  // reading from socket
  int bytes_read = read(socket, self->read_buffer + self->read_index, bytes_available);

  if (bytes_read < 0) {
    printf("error during reading errno: %i\n", errno);
    return NULL;
  }

  self->read_index += bytes_read;
  pb_istream_t stream = pb_istream_from_buffer(self->read_buffer, self->read_index);

  if (!pb_decode(&stream, PortMessage_fields, &self->output)) {
    printf("decoding failed: %s\n", stream.errmsg);
    return NULL;
  }

  return &self->output;
}

void TcpIpBundle_close(TcpIpBundle *self) {
  if (self->server) {
    close(self->client);
  }
  close(self->fd);
}

void TcpIpBundle_change_block_state(TcpIpBundle *self, bool blocking) {
  if (blocking) {
    fcntl(self->fd, F_SETFL, fcntl(self->fd, F_GETFL) | (~O_NONBLOCK));

    if (self->server) {
      fcntl(self->client, F_SETFL, fcntl(self->fd, F_GETFL) | (~O_NONBLOCK));
    }
  } else {
    // configure the socket to be non-blocking

    fcntl(self->fd, F_SETFL, fcntl(self->fd, F_GETFL) | O_NONBLOCK);

    if (self->server) {
      fcntl(self->client, F_SETFL, fcntl(self->fd, F_GETFL) | O_NONBLOCK);
    }
  }
}

void TcpIpBundle_ctor(TcpIpBundle *self, const char *host, unsigned short port, int protocol_family) {
  FD_ZERO(&self->set);

  if ((self->fd = socket(protocol_family, SOCK_STREAM, 0)) < 0) {
    exit(1);
  }

  self->server = true;
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
}
