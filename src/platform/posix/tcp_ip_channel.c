#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/logging.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
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

static lf_ret_t TcpIpChannel_reset_socket(TcpIpChannel *self) {
  FD_ZERO(&self->set);
  if (self->fd > 0) {
    if (close(self->fd) < 0) {
      LF_ERR(NET, "Error closing socket %d", errno);
      return LF_ERR;
    }
  }

  if ((self->fd = socket(self->protocol_family, SOCK_STREAM, 0)) < 0) {
    LF_ERR(NET, "Error opening socket %d", errno);
    return LF_ERR;
  }

  if (setsockopt(self->fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    LF_ERR(NET, "Error setting socket options %d", errno);
    return LF_ERR;
  }

  // Set server socket to non-blocking
  fcntl(self->fd, F_SETFL, O_NONBLOCK);

  return LF_OK;
}
/**
 * @brief If is server: Bind and Listen for connections
 * If is client: Do nothing
 */
static lf_ret_t TcpIpChannel_open_connection(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  if (self->server) {
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
  }

  return LF_OK;
}

static lf_ret_t TcpIpChannel_try_connect_server(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  int new_socket;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);

  new_socket = accept(self->fd, (struct sockaddr *)&address, &addrlen);
  if (new_socket >= 0) {
    self->client = new_socket;
    FD_SET(new_socket, &self->set);
    return LF_OK;
  } else {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      LF_DEBUG(NET, "Accept failed. Try again. (errno=%d)", errno);
      return LF_TRY_AGAIN;
    } else {
      LF_ERR(NET, "Accept failed. Unknown errno=%d", errno);
      throw("Accept failed");
      return LF_ERR;
    }
  }
}

static lf_ret_t TcpIpChannel_check_if_socket_is_writable(int fd) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd, &set);
  struct timeval tv = {.tv_sec = 0, .tv_usec = 1000};

  int ret = select(fd + 1, NULL, &set, NULL, &tv);
  if (ret > 0) {
    if (FD_ISSET(fd, &set)) {
      return LF_OK;
    } else {
      LF_DEBUG(NET, "Select: socket not writable yet");
      return LF_TRY_AGAIN;
    }
  } else if (ret == 0) {
    // Timeout
    LF_DEBUG(NET, "Select timed out");
    return LF_TIMEOUT;
  } else {
    LF_DEBUG(NET, "Select failed with errno=%d", errno);
    return LF_ERR;
  }
}

static lf_ret_t TcpIpChannel_check_socket_error(int fd) {
  int so_error;
  socklen_t len = sizeof(so_error);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
    return LF_ERR;
  }
  if (so_error == 0) {
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

static lf_ret_t TcpIpChannel_try_connect_client(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  lf_ret_t lf_ret;

  if (!self->client_connect_in_progress) {
    // First time trying to connect
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = self->protocol_family;
    serv_addr.sin_port = htons(self->port);

    if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
      return LF_INVALID_VALUE;
    }

    int ret = connect(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
      if (errno == EINPROGRESS) {
        self->client_connect_in_progress = true;
        LF_DEBUG(NET, "Connection in progress!");
        return LF_IN_PROGRESS;
      } else {
        LF_ERR(NET, "Connect failed errno=%d", errno);
        self->client_connect_in_progress = false;
        TcpIpChannel_reset_socket(self);
        return LF_TRY_AGAIN;
      }
    }
  } else {
    // Connection is in progress
    lf_ret = TcpIpChannel_check_if_socket_is_writable(self->fd);
    if (lf_ret == LF_OK) {
      LF_DEBUG(NET, "Socket is writable");
      lf_ret = TcpIpChannel_check_socket_error(self->fd);
      if (lf_ret == LF_OK) {
        LF_DEBUG(NET, "Connection succeeded");
        self->client_connect_in_progress = false;
        return LF_OK;
      } else {
        self->client_connect_in_progress = false;
        LF_ERR(NET, "Connection failed");
        TcpIpChannel_reset_socket(self);
        return LF_TRY_AGAIN;
      }
    } else if (lf_ret == LF_TIMEOUT) {
      LF_ERR(NET, "Select timed out");
      return LF_IN_PROGRESS;
    } else {
      self->client_connect_in_progress = false;
      TcpIpChannel_reset_socket(self);
      LF_ERR(NET, "Select failed errno=%d", errno);
      return LF_TRY_AGAIN;
    }
  }
  return LF_ERR; // Should never reach here
}

static lf_ret_t TcpIpChannel_try_connect(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  if (self->server) {
    return TcpIpChannel_try_connect_server(untyped_self);
  } else {
    return TcpIpChannel_try_connect_client(untyped_self);
  }
}

static lf_ret_t TcpIpChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
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
  int message_size = serialize_to_protobuf(message, self->write_buffer, TCP_IP_CHANNEL_BUFFERSIZE);

  if (message_size < 0) {
    LF_ERR(NET, "Could not encode protobuf");
    return LF_ERR;
  }

  // sending serialized data
  ssize_t bytes_written = 0;
  int timeout = TCP_IP_CHANNEL_NUM_RETRIES;

  while (bytes_written < message_size && timeout > 0) {
    LF_DEBUG(NET, "Sending %d bytes", message_size - bytes_written);
    ssize_t bytes_send = send(socket, self->write_buffer + bytes_written, message_size - bytes_written, 0);
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

const FederateMessage *TcpIpChannel_receive(NetworkChannel *untyped_self) {
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
  int bytes_left;
  bool read_more = true;

  while (read_more) {
    // reading from socket
    ssize_t bytes_read = recv(socket, self->read_buffer + self->read_index, bytes_available, 0);

    if (bytes_read < 0) {
      LF_ERR(NET, "[%s] Error recv from socket %d", self->server ? "server" : "client", errno);
      continue;
    }

    self->read_index += bytes_read;
    bytes_left = deserialize_from_protobuf(&self->output, self->read_buffer, self->read_index);
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

static void TcpIpChannel_close_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "Closing TCP/IP Channel");
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  if (self->server && self->client != 0) {
    if (close(self->client) < 0) {
      LF_ERR(NET, "Error closing client socket %d", errno);
    }
  }

  if (close(self->fd) < 0) {
    LF_ERR(NET, "Error closing server socket %d", errno);
  }
}

static void *TcpIpChannel_receive_thread(void *untyped_self) {
  LF_INFO(NET, "Starting TCP/IP receive thread");
  TcpIpChannel *self = untyped_self;

  // set terminate to false so the loop runs
  self->terminate = false;

  while (!self->terminate) {
    const FederateMessage *msg = TcpIpChannel_receive(untyped_self);

    if (msg) {
      self->receive_callback(self->federated_connection, msg);
    }
  }

  return NULL;
}

static void TcpIpChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                   void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                            const FederateMessage *msg),
                                                   FederatedConnectionBundle *conn) {
  int res;
  LF_INFO(NET, "TCP/IP registering callback thread");
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  int fd;
  if (self->server) {
    fd = self->client;
  } else {
    fd = self->fd;
  }

  // Set socket to blocking
  int opts = fcntl(fd, F_GETFL);
  if (opts < 0) {
    throw("Could not get socket options");
  }
  opts = (opts & (~O_NONBLOCK));
  if (fcntl(fd, F_SETFL, opts) < 0) {
    throw("Could not set socket to blocking");
  }

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
  memset(&self->receive_thread_stack, 0, TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE);
  if (pthread_attr_init(&self->receive_thread_attr) != 0) {
    throw("pthread_attr_init failed");
  }
/* TODO: RIOT posix-wrappers don't have pthread_attr_setstack yet */
#if defined(PLATFORM_RIOT) && !defined(__USE_XOPEN2K)
  if (pthread_attr_setstackaddr(&self->receive_thread_attr, self->receive_thread_stack) != 0) {
    throw("pthread_attr_setstackaddr failed");
  }
  if (pthread_attr_setstacksize(&self->receive_thread_attr, TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE -
                                                                TCP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE) != 0) {
    throw("pthread_attr_setstacksize failed");
  }
#else
  if (pthread_attr_setstack(&self->receive_thread_attr, &self->receive_thread_stack,
                            TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE - TCP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE) < 0) {
    throw("pthread_attr_setstack failed");
  }
#endif
  res = pthread_create(&self->receive_thread, &self->receive_thread_attr, TcpIpChannel_receive_thread, self);
  if (res < 0) {
    throw("pthread_create failed");
  }
}

static void TcpIpChannel_free(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "Freeing TCP/IP Channel");
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  self->terminate = true;

  if (self->receive_thread != 0) {
    int err = 0;
    LF_DEBUG(NET, "Stopping receive thread");

    err = pthread_cancel(self->receive_thread);
    if (err != 0) {
      LF_ERR(NET, "Error canceling receive thread %d", err);
    }

    err = pthread_join(self->receive_thread, NULL);
    if (err != 0) {
      LF_ERR(NET, "Error joining receive thread %d", err);
    }

    err = pthread_attr_destroy(&self->receive_thread_attr);
    if (err != 0) {
      LF_ERR(NET, "Error destroying pthread attr %d", err);
    }
  }
  self->super.close_connection((NetworkChannel *)self);
}

void TcpIpChannel_ctor(TcpIpChannel *self, const char *host, unsigned short port, int protocol_family, bool server) {

  self->server = server;
  self->terminate = true;
  self->protocol_family = protocol_family;
  self->host = host;
  self->port = port;
  self->read_index = 0;
  self->client = 0;
  self->fd = 0;
  self->client_connect_in_progress = false;

  self->super.open_connection = TcpIpChannel_open_connection;
  self->super.try_connect = TcpIpChannel_try_connect;
  self->super.close_connection = TcpIpChannel_close_connection;
  self->super.send_blocking = TcpIpChannel_send_blocking;
  self->super.register_receive_callback = TcpIpChannel_register_receive_callback;
  self->super.free = TcpIpChannel_free;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->receive_thread = 0;

  TcpIpChannel_reset_socket(self);
}