#include "reactor-uc/platform/zephyr/tcp_ip_channel.h"
#include "reactor-uc/encoding.h"
#include "reactor-uc/logging.h"

#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>

#include "proto/message.pb.h"

#include <errno.h>
#include <sys/fcntl.h>
#include <zephyr/net/socket.h>

#define NUM_TCP_IP_CHANNELS 4 // FIXME: This must be declared by the user/compiler.
#define RECEIVE_THREAD_STACKSIZE 1024

#ifndef NUM_TCP_IP_CHANNELS
#define NUM_TCP_IP_CHANNELS 0
#endif

#define RECEIVE_THREAD_PRIORITY 5 // TODO: What should the priority be? 0 is highest.

// TODO: Making these global variables assumes that there will only be a single environment running at a time.
K_THREAD_STACK_ARRAY_DEFINE(receive_thread_stack, NUM_TCP_IP_CHANNELS, RECEIVE_THREAD_STACKSIZE);
static struct k_thread receive_thread_data[NUM_TCP_IP_CHANNELS];
static size_t receive_thread_stack_idx = 0;

lf_ret_t TcpIpChannel_bind(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  struct sockaddr_in serv_addr;
  (void)memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  // bind the socket to that address
  int ret = bind(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (ret < 0) {
    LF_ERR(NET, "Could not bind to socket %d errno=%d", ret, errno);
    return LF_NETWORK_SETUP_FAILED;
  }

  // start listening. Will only accept a single client.
  if (listen(self->fd, 1) < 0) {
    LF_ERR(NET, "Could not listen to socket errno=%d", errno);
    return LF_NETWORK_SETUP_FAILED;
  }

  return LF_OK;
}

lf_ret_t TcpIpChannel_connect(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  self->server = false;

  struct sockaddr_in serv_addr;
  (void)memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);

  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    return LF_INVALID_VALUE;
  }

  int ret = connect(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (ret < 0) {
    LF_ERR(NET, "Client could not connect to socket errno=%d", errno);
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
  LF_WARN(NET, "accept failed");
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
  LF_DEBUG(NET, "Encoded message of size %d", message_size);

  if (message_size < 0) {
    LF_ERR(NET, "Could not encode protobuf");
    return LF_ERR;
  }

  // sending serialized data
  ssize_t bytes_written = 0;
  int timeout = TCP_IP_CHANNEL_NUM_RETRIES;

  while (bytes_written < message_size && timeout > 0) {
    LF_DEBUG(NET, "Sending %d bytes", message_size - bytes_written);
    int bytes_send = write(socket, self->write_buffer + bytes_written, message_size - bytes_written);

    if (bytes_send < 0) {
      LF_ERR(NET, "write failed errno=%d", errno);
      return LF_ERR;
    }

    bytes_written += bytes_send;
    timeout--;
  }

  // checking if the whole message was transmitted or timeout was received
  if (timeout == 0 || bytes_written < message_size) {
    LF_ERR(NET, "write timed out.");
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
  int max_read_size = TCP_IP_CHANNEL_BUFFERSIZE - self->read_index;

  // reading from socket
  // printf("Trying to read %d bytes\n", max_read_size);
  int bytes_read = recv(socket, self->read_buffer + self->read_index, max_read_size, 0);

  if (bytes_read < 0) {
    // LF_ERR(NET, "read faile %d errno=%d", bytes_read, errno);
    return NULL;
  }

  self->read_index += bytes_read;

  int bytes_left = decode_protobuf(&self->output, self->read_buffer, self->read_index);

  if (bytes_left < 0) {
    LF_WARN(NET, "Could not decode incoming protobuf. Try receiving more data");
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

void TcpIpChannel_receive_thread(void *untyped_self, void *unused, void *unused2) {
  (void)unused;
  (void)unused2;
  TcpIpChannel *self = untyped_self;

  printf("RECEIVE THREAD STARTED\n");

  LF_DEBUG(NET, "Starting receive thread");
  // set terminate to false so the loop runs
  self->terminate = false;

  while (!self->terminate) {
    TaggedMessage *msg = self->super.receive(untyped_self);
    if (msg) {
      self->receive_callback(self->federated_connection, msg);
    }
  }
}

void TcpIpChannel_register_callback(NetworkChannel *untyped_self,
                                    void (*receive_callback)(FederatedConnectionBundle *conn, TaggedMessage *msg),
                                    FederatedConnectionBundle *conn) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;

  self->receive_thread =
      k_thread_create(&receive_thread_data[receive_thread_stack_idx], receive_thread_stack[receive_thread_stack_idx],
                      RECEIVE_THREAD_STACKSIZE, TcpIpChannel_receive_thread, (void *)self, NULL, NULL,
                      RECEIVE_THREAD_PRIORITY, 0, K_NO_WAIT);
}

void TcpIpChannel_ctor(TcpIpChannel *self, const char *host, unsigned short port, int protocol_family) {
  FD_ZERO(&self->set);
  if ((self->fd = socket(protocol_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    LF_ERR(NET, "Could not create socket errno=%d", errno);
    validate(false);
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
    k_thread_join(self->receive_thread, K_FOREVER);
  }
  self->super.close((NetworkChannel *)self);
}