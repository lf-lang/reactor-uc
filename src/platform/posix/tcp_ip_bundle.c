#include "reactor-uc/platform/posix/tcp_ip_bundle.h"

#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#include <nanopb/pb_encode.h>
#include <nanopb/pb_decode.h>

#include "reactor-uc/generated/message.pb.h"


void TcpIpBundle_bind(TcpIpBundle* self) {
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  // turn human-readable address into something the os can work with
  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    exit(1);
  }

  // bind the socket to that address
  if (bind(self->fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    exit(1);
  }

  // start listening
  if (listen(self->fd, 1) < 0) {
    exit(1);
  }
}


void TcpIpBundle_connect(TcpIpBundle* self) {
  struct sockaddr_in serv_addr;

  fcntl(self->fd, F_SETFL, fcntl(self->fd, F_GETFL) | O_NONBLOCK);

  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    printf("convert addr failed %s\n", self->host);
  }

  if (connect(self->fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("connect failed %i\n", errno);
  }

  self->server = false;
}

bool TcpIpBundle_accept(TcpIpBundle* self) {
  if (self->clients_size >= self->clients_capacity) {
    return false;
  }

  int new_socket;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);

  if ((new_socket = accept(self->fd, (struct sockaddr*)&address, &addrlen)) > 0) {
    self->clients[self->clients_size++] = new_socket;
    FD_SET(new_socket, &self->set);

    return true;
  } else {

    printf("no connection to accept\n");
    return false;
  }
}


void TcpIpBundle_send(TcpIpBundle* self, PortMessage* message) {
  int socket;

  // based if this bundle is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->clients[self->clients_index];
    self->clients_index = (self->clients_index + 1) % self->clients_size;
  } else {
    socket = self->fd;
  }

  pb_ostream_t stream = pb_ostream_from_buffer(self->write_buffer, sizeof(self->write_buffer));
  int status = pb_encode(&stream, PortMessage_fields, message);

  if (status < 0) {
    printf("encoding failed!\n");
    // message wasn't able to be encoded
  }

  int bytes_written;

  bytes_written = write(socket, stream.state, stream.bytes_written);
  printf("writing into %zu actually written %i\n", stream.bytes_written, bytes_written);
  if (bytes_written < stream.bytes_written) {
    printf("not enough bytes written %i %zu \n", bytes_written, stream.bytes_written);
    // not enough bytes written
  }
}

PortMessage*TcpIpBundle_receive(TcpIpBundle* self) {
  int bytes_available;
  int socket;

  // based if this bundle is in the server or client role we need to select different sockets
  if (self->server) {
    socket = self->clients[self->clients_index];
    self->clients_index = (self->clients_index + 1) % self->clients_size;
  } else {
    socket = self->fd;
  }

  // peek into file descriptor to figure how many bytes are available.
  ioctl(socket,FIONREAD, &bytes_available);

  if (bytes_available == 0) {
    return NULL;
  }

  // calculating the maximum amount of bytes we can read
  if (bytes_available + self->read_index >= 1024) {
    bytes_available = 1024 - self->read_index;
  }

  // reading from socket
  int bytes_read = read(self->fd, self->read_buffer + self->read_index, bytes_available);

  if (bytes_read < 0) {
    printf("error during reading errno: %i\n", errno);
  }

  printf("requested %i reading %i from %i\n", bytes_available, bytes_read, self->fd);
  self->read_index += bytes_read;

  printf("bytes read %i bytes avail: %i\n", self->read_index, bytes_available);
  pb_istream_t stream = pb_istream_from_buffer(self->write_buffer, self->read_index);

  if(!pb_decode(&stream, PortMessage_fields, &self->output)) {
    printf("decoding failed\n");
  }

  return &self->output;
}

void TcpIpBundle_close(TcpIpBundle* self) {
  for (int i = 0; i < self->clients_size; i++) {
    close(self->clients[i]);
  }

  close(self->fd);
}


void TcpIpBundle_Server_Ctor(TcpIpBundle* self, const char* host, unsigned short port, int protocol_family) {
  FD_ZERO(&self->set);

  if ((self->fd = socket(protocol_family, SOCK_STREAM, 0)) < 0) {
    exit(1);
  }

  // configure the socket to be non-blocking
  fcntl(self->fd, F_SETFL, fcntl(self->fd, F_GETFL) | O_NONBLOCK);

  self->server = true;
  self->protocol_family = protocol_family;
  self->host = host;
  self->port = port;
  self->read_index = 0;
  self->clients_size = 0;
  self->clients_capacity = 20;
  self->clients_index = 0;

  self->accept = TcpIpBundle_accept;
  self->bind = TcpIpBundle_bind;
  self->connect = TcpIpBundle_connect;
  self->close = TcpIpBundle_close;
  self->receive = TcpIpBundle_receive;
  self->send = TcpIpBundle_send;
}
