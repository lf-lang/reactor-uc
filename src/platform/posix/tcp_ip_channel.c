#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/federated.h"

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

#define TCP_IP_CHANNEL_ERR(fmt, ...)                                                                                   \
  LF_ERR(NET, "TcpIpChannel: [%s] " fmt, self->is_server ? "server" : "client", ##__VA_ARGS__)
#define TCP_IP_CHANNEL_WARN(fmt, ...)                                                                                  \
  LF_WARN(NET, "TcpIpChannel: [%s] " fmt, self->is_server ? "server" : "client", ##__VA_ARGS__)
#define TCP_IP_CHANNEL_INFO(fmt, ...)                                                                                  \
  LF_INFO(NET, "TcpIpChannel: [%s] " fmt, self->is_server ? "server" : "client", ##__VA_ARGS__)
#define TCP_IP_CHANNEL_DEBUG(fmt, ...)                                                                                 \
  LF_DEBUG(NET, "TcpIpChannel: [%s] " fmt, self->is_server ? "server" : "client", ##__VA_ARGS__)

// Forward declarations
static void *_TcpIpChannel_worker_thread(void *untyped_self);

static void _TcpIpChannel_update_state(TcpIpChannel *self, NetworkChannelState new_state) {
  TCP_IP_CHANNEL_DEBUG("Update state: %s => %s", NetworkChannel_state_to_string(self->state),
                       NetworkChannel_state_to_string(new_state));

  pthread_mutex_lock(&self->mutex);
  // Store old state
  NetworkChannelState old_state = self->state;

  // Update the state of the channel to its new state
  self->state = new_state;
  pthread_mutex_unlock(&self->mutex);

  if (new_state == NETWORK_CHANNEL_STATE_CONNECTED) {
    self->was_ever_connected = true;
  }

  // Inform runtime about new state if it changed from or to NETWORK_CHANNEL_STATE_CONNECTED
  if ((old_state == NETWORK_CHANNEL_STATE_CONNECTED && new_state != NETWORK_CHANNEL_STATE_CONNECTED) ||
      (old_state != NETWORK_CHANNEL_STATE_CONNECTED && new_state == NETWORK_CHANNEL_STATE_CONNECTED)) {
    _lf_environment->platform->new_async_event(_lf_environment->platform);
  }
  TCP_IP_CHANNEL_DEBUG("Update state: %s => %s done", NetworkChannel_state_to_string(self->state),
                       NetworkChannel_state_to_string(new_state));
}

static NetworkChannelState _TcpIpChannel_get_state(TcpIpChannel *self) {
  NetworkChannelState state;

  pthread_mutex_lock(&self->mutex);
  state = self->state;
  pthread_mutex_unlock(&self->mutex);

  return state;
}

static lf_ret_t _TcpIpChannel_reset_socket(TcpIpChannel *self) {
  FD_ZERO(&self->set);
  if (self->fd > 0) {
    if (close(self->fd) < 0) {
      TCP_IP_CHANNEL_ERR("Error closing socket errno=%d", errno);
      return LF_ERR;
    }
  }
  if (self->send_failed_event_fds[0] > 0) {
    if (close(self->send_failed_event_fds[0]) < 0) {
      TCP_IP_CHANNEL_ERR("Error closing send_failed_event_fds[0] errno=%d", errno);
      return LF_ERR;
    }
  }
  if (self->send_failed_event_fds[1] > 0) {
    if (close(self->send_failed_event_fds[1]) < 0) {
      TCP_IP_CHANNEL_ERR("Error closing send_failed_event_fds[1] errno=%d", errno);
      return LF_ERR;
    }
  }

  if ((self->fd = socket(self->protocol_family, SOCK_STREAM, 0)) < 0) {
    TCP_IP_CHANNEL_ERR("Error opening socket errno=%d", errno);
    return LF_ERR;
  }

  if (setsockopt(self->fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    TCP_IP_CHANNEL_ERR("Error setting socket options: SO_REUSEADDR errno=%d", errno);
    return LF_ERR;
  }

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, self->send_failed_event_fds) < 0) {
    TCP_IP_CHANNEL_ERR("Failed to initialize \"send_failed\" socketpair file descriptors");
    return LF_ERR;
  }

  return LF_OK;
}

static void _TcpIpChannel_spawn_worker_thread(TcpIpChannel *self) {
  int res;
  TCP_IP_CHANNEL_INFO("Spawning worker thread");

  // set terminate to false so the loop runs
  self->terminate = false;

  memset(&self->worker_thread_stack, 0, TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE);
  if (pthread_attr_init(&self->worker_thread_attr) != 0) {
    throw("pthread_attr_init failed");
  }
  if (pthread_attr_setstack(&self->worker_thread_attr, &self->worker_thread_stack,
                            TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE - TCP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE) < 0) {
    throw("pthread_attr_setstack failed");
  }

  res = pthread_create(&self->worker_thread, &self->worker_thread_attr, _TcpIpChannel_worker_thread, self);
  if (res < 0) {
    throw("pthread_create failed");
  }
}

static lf_ret_t _TcpIpChannel_server_bind(TcpIpChannel *self) {
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = self->protocol_family;
  serv_addr.sin_port = htons(self->port);
  TCP_IP_CHANNEL_INFO("Bind to %s:%u", self->host, self->port);

  // turn human-readable address into something the os can work with
  if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
    TCP_IP_CHANNEL_ERR("Invalid address %s", self->host);
    return LF_INVALID_VALUE;
  }

  // bind the socket to that address
  int ret = bind(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (ret < 0) {
    TCP_IP_CHANNEL_ERR("Could not bind to %s:%d errno=%d", self->host, self->port, errno);
    throw("Bind failed");
    _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
    return LF_ERR;
  }

  // start listening
  if (listen(self->fd, 1) < 0) {
    TCP_IP_CHANNEL_ERR("Could not listen to %s:%d", self->host, self->port);
    _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
    return LF_ERR;
  }

  return LF_OK;
}

static lf_ret_t _TcpIpChannel_try_connect_server(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  int new_socket;
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);

  new_socket = accept(self->fd, (struct sockaddr *)&address, &addrlen);
  if (new_socket >= 0) {
    self->client = new_socket;
    FD_SET(new_socket, &self->set);
    TCP_IP_CHANNEL_INFO("Connceted to client with address %s", inet_ntoa(address.sin_addr));
    _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
    return LF_OK;
  } else {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      if (!self->has_warned_about_connection_failure) {
        TCP_IP_CHANNEL_WARN("Accept failed with errno=%d. Will only print one warning.", errno);
        self->has_warned_about_connection_failure = true;
      }
      return LF_OK;
    } else {
      TCP_IP_CHANNEL_ERR("Accept failed. errno=%d", errno);
      throw("Accept failed");
      return LF_ERR;
    }
  }
}

static lf_ret_t _TcpIpChannel_try_connect_client(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  if (_TcpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_OPEN) {
    // First time trying to connect
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = self->protocol_family;
    serv_addr.sin_port = htons(self->port);

    if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
      return LF_INVALID_VALUE;
    }

    int ret = connect(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret == 0) {
      TCP_IP_CHANNEL_INFO("Connected to server on %s:%d", self->host, self->port);
      _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
      return LF_OK;
    } else {
      if (errno == EINPROGRESS) {
        _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS);
        TCP_IP_CHANNEL_DEBUG("Connection in progress!");
        return LF_OK;
      } else {
        if (!self->has_warned_about_connection_failure) {
          TCP_IP_CHANNEL_WARN("Connect to %s:%d failed with errno=%d. Will only print one error.", self->host,
                              self->port, errno);
          self->has_warned_about_connection_failure = true;
        }
        _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
        return LF_ERR;
      }
    }
  } else {
    TCP_IP_CHANNEL_ERR("try_connect_client called in invalid state %d", _TcpIpChannel_get_state(self));
    return LF_ERR;
  }

  return LF_ERR; // Should never reach here
}

static lf_ret_t TcpIpChannel_open_connection(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  TCP_IP_CHANNEL_DEBUG("Open connection");

  _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_OPEN);

  return LF_OK;
}

static lf_ret_t TcpIpChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  TCP_IP_CHANNEL_DEBUG("Send blocking");
  lf_ret_t lf_ret = LF_ERR;

  if (_TcpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED) {
    int socket;

    // based if this super is in the server or client role we need to select different sockets
    if (self->is_server) {
      socket = self->client;
    } else {
      socket = self->fd;
    }

    // serializing protobuf into buffer
    int message_size = serialize_to_protobuf(message, self->write_buffer, TCP_IP_CHANNEL_BUFFERSIZE);

    if (message_size < 0) {
      TCP_IP_CHANNEL_ERR("Could not encode protobuf");
      lf_ret = LF_ERR;
    } else {
      // sending serialized data
      ssize_t bytes_written = 0;
      int timeout = TCP_IP_CHANNEL_NUM_RETRIES;

      while (bytes_written < message_size && timeout > 0) {
        TCP_IP_CHANNEL_DEBUG("Sending %d bytes", message_size - bytes_written);
        ssize_t bytes_send = send(socket, self->write_buffer + bytes_written, message_size - bytes_written, 0);
        TCP_IP_CHANNEL_DEBUG("%d bytes sent", bytes_send);

        if (bytes_send < 0) {
          TCP_IP_CHANNEL_ERR("Write failed errno=%d", errno);
          switch (errno) {
          case ETIMEDOUT:
          case ENOTCONN: {
            ssize_t bytes_written = write(self->send_failed_event_fds[1], "X", 1);
            if (bytes_written == -1) {
              TCP_IP_CHANNEL_ERR("Failed informing worker thread, that send_blocking failed, errno=%d", errno);
            }
            lf_ret = LF_ERR;
            break;
          }
          default:
            lf_ret = LF_ERR;
          }
        } else {
          bytes_written += bytes_send;
          timeout--;
          lf_ret = LF_OK;
        }
      }

      // checking if the whole message was transmitted or timeout was received
      if (timeout == 0 || bytes_written < message_size) {
        TCP_IP_CHANNEL_ERR("Timeout on sending message");
        lf_ret = LF_ERR;
      }
    }
  }

  return lf_ret;
}

static lf_ret_t _TcpIpChannel_receive(NetworkChannel *untyped_self, FederateMessage *return_message) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  int socket;

  // based if this super is in the server or client role we need to select different sockets
  if (self->is_server) {
    socket = self->client;
  } else {
    socket = self->fd;
  }

  // calculating the maximum amount of bytes we can read
  int bytes_available = TCP_IP_CHANNEL_BUFFERSIZE - self->read_index;
  int bytes_left;
  bool read_more = true;

  if (self->read_index > 0) {
    TCP_IP_CHANNEL_DEBUG("Has %d bytes in read_buffer from last recv. Trying to deserialize", self->read_index);
    bytes_left = deserialize_from_protobuf(&self->output, self->read_buffer, self->read_index);
    if (bytes_left >= 0) {
      TCP_IP_CHANNEL_DEBUG("%d bytes left after deserialize", bytes_left);
      read_more = false;
    }
  }

  while (read_more) {
    // reading from socket
    ssize_t bytes_read = recv(socket, self->read_buffer + self->read_index, bytes_available, 0);

    if (bytes_read < 0) {
      switch (errno) {
      case ETIMEDOUT:
      case ECONNRESET:
      case ENOTCONN:
      case ECONNABORTED:
        TCP_IP_CHANNEL_ERR("Error recv from socket errno=%d", errno);
        _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
        return LF_ERR;
      case EAGAIN:
        /* The socket has no new data to receive */
        return LF_EMPTY;
      }
      continue;
    } else if (bytes_read == 0) {
      // This means the connection was closed.
      _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CLOSED);
      TCP_IP_CHANNEL_WARN("Other federate closed socket");
      return LF_ERR;
    }

    TCP_IP_CHANNEL_DEBUG("Read %d bytes from socket %d", bytes_read, socket);

    self->read_index += bytes_read;
    bytes_left = deserialize_from_protobuf(return_message, self->read_buffer, self->read_index);
    TCP_IP_CHANNEL_DEBUG("%d bytes left after deserialize", bytes_left);
    if (bytes_left < 0) {
      read_more = true;
    } else {
      read_more = false;
    }
  }

  memcpy(self->read_buffer, self->read_buffer + (self->read_index - bytes_left), bytes_left);
  self->read_index = bytes_left;

  return LF_OK;
}

static void TcpIpChannel_close_connection(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  TCP_IP_CHANNEL_DEBUG("Closing connection");

  if (self->state != NETWORK_CHANNEL_STATE_CONNECTED) {
    return;
  }

  if (self->is_server && self->client != 0) {
    if (close(self->client) < 0) {
      TCP_IP_CHANNEL_ERR("Error closing client socket %d", errno);
    }
  }

  if (close(self->fd) < 0) {
    TCP_IP_CHANNEL_ERR("Error closing server socket %d", errno);
  }
}

/**
 * @brief Main loop of the TcpIpChannel.
 */
static void *_TcpIpChannel_worker_thread(void *untyped_self) {
  TcpIpChannel *self = untyped_self;
  lf_ret_t ret;

  TCP_IP_CHANNEL_INFO("Starting worker thread");

  while (!self->terminate) {
    switch (_TcpIpChannel_get_state(self)) {
    case NETWORK_CHANNEL_STATE_OPEN: {
      /* try to connect */
      if (self->is_server) {
        ret = _TcpIpChannel_server_bind(self);
        if (ret != LF_OK) {
          _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
          break;
        }
        _TcpIpChannel_try_connect_server(untyped_self);
      } else {
        _TcpIpChannel_try_connect_client(untyped_self);
      }
    } break;

    case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS: {
      _lf_environment->platform->wait_for(_lf_environment->platform, self->super.expected_connect_duration);
    } break;

    case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
    case NETWORK_CHANNEL_STATE_CONNECTION_FAILED: {
      _lf_environment->platform->wait_for(_lf_environment->platform, self->super.expected_connect_duration);
      _TcpIpChannel_reset_socket(self);
      _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_OPEN);
    } break;

    case NETWORK_CHANNEL_STATE_CONNECTED: {
      int socket = 0;
      if (self->is_server) {
        socket = self->client;
      } else {
        socket = self->fd;
      }

      fd_set readfds;
      int max_fd;

      // Set up the file descriptor set
      FD_ZERO(&readfds);
      FD_SET(socket, &readfds);
      FD_SET(self->send_failed_event_fds[0], &readfds);

      // Determine the maximum file descriptor for select
      max_fd = (socket > self->send_failed_event_fds[0]) ? socket : self->send_failed_event_fds[0];

      // Wait for data or cancel if send_failed externally
      if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
        TCP_IP_CHANNEL_ERR("Select returned with error. errno=", errno);
        break;
      }

      if (FD_ISSET(socket, &readfds)) {
        TCP_IP_CHANNEL_DEBUG("Select -> receive");
        ret = _TcpIpChannel_receive(untyped_self, &self->output);
        if (ret == LF_EMPTY) {
          /* The non-blocking recv has no new data yet */
        } else if (ret == LF_OK) {
          validate(self->receive_callback);
          self->receive_callback(self->federated_connection, &self->output);
        } else if (ret == LF_ERR) {
          _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
        }
      } else if (FD_ISSET(self->send_failed_event_fds[0], &readfds)) {
        TCP_IP_CHANNEL_DEBUG("Select -> cancelled by send_block failure");
        _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
      }

    } break;

    case NETWORK_CHANNEL_STATE_UNINITIALIZED:
    case NETWORK_CHANNEL_STATE_CLOSED:
      break;
    }
  }

  TCP_IP_CHANNEL_INFO("Worker thread terminates");
  return NULL;
}

static void TcpIpChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                   void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                            const FederateMessage *msg),
                                                   FederatedConnectionBundle *conn) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  TCP_IP_CHANNEL_DEBUG("Register receive callback");
  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

static void TcpIpChannel_free(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  TCP_IP_CHANNEL_DEBUG("Free");
  self->terminate = true;

  if (self->worker_thread != 0) {
    int err = 0;
    TCP_IP_CHANNEL_DEBUG("Stopping worker thread");

    err = pthread_cancel(self->worker_thread);

    if (err != 0) {
      TCP_IP_CHANNEL_ERR("Error canceling worker thread %d", err);
    }

    err = pthread_join(self->worker_thread, NULL);
    if (err != 0) {
      TCP_IP_CHANNEL_ERR("Error joining worker thread %d", err);
    }

    err = pthread_attr_destroy(&self->worker_thread_attr);
    if (err != 0) {
      TCP_IP_CHANNEL_ERR("Error destroying pthread attr %d", err);
    }
  }
  self->super.close_connection((NetworkChannel *)self);
  pthread_mutex_destroy(&self->mutex);
}

static bool TcpIpChannel_is_connected(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;

  return _TcpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED;
}

static bool TcpIpChannel_was_ever_connected(NetworkChannel *untyped_self) {
  TcpIpChannel *self = (TcpIpChannel *)untyped_self;
  return self->was_ever_connected;
}

void TcpIpChannel_ctor(TcpIpChannel *self, const char *host, unsigned short port, int protocol_family, bool is_server) {
  assert(self != NULL);
  assert(host != NULL);

  if (pthread_mutex_init(&self->mutex, NULL) != 0) {
    throw("Failed to initialize mutex");
  }

  self->is_server = is_server;
  self->terminate = true;
  self->protocol_family = protocol_family;
  self->host = host;
  self->port = port;
  self->read_index = 0;
  self->client = 0;
  self->fd = 0;
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;

  self->super.is_connected = TcpIpChannel_is_connected;
  self->super.was_ever_connected = TcpIpChannel_was_ever_connected;
  self->super.open_connection = TcpIpChannel_open_connection;
  self->super.close_connection = TcpIpChannel_close_connection;
  self->super.send_blocking = TcpIpChannel_send_blocking;
  self->super.register_receive_callback = TcpIpChannel_register_receive_callback;
  self->super.free = TcpIpChannel_free;
  self->super.expected_connect_duration = TCP_IP_CHANNEL_EXPECTED_CONNECT_DURATION; // Needed for Zephyr
  self->super.type = NETWORK_CHANNEL_TYPE_TCP_IP;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->worker_thread = 0;
  self->has_warned_about_connection_failure = false;
  self->was_ever_connected = false;

  _TcpIpChannel_reset_socket(self);
  _TcpIpChannel_spawn_worker_thread(self);
}
