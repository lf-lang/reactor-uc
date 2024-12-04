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

static bool _is_globals_initialized = false;
static Environment *_env;

static bool _is_globals_initialized = false;
static Environment *_env;

// Forward declarations
static void _spawn_worker_thread(TcpIpChannel *self);
static lf_ret_t _reset_socket(TcpIpChannel *self);
static void *_worker_thread(void *untyped_self);
static void _spawn_worker_thread(TcpIpChannel *self);
static lf_ret_t _reset_socket(TcpIpChannel *self);
static void *_worker_thread(void *untyped_self);

static void _update_state(TcpIpChannel *self, NetworkChannelState new_state) {
  // Update the state of the channel itself
  self->state = new_state;

  // Inform runtime about new state
  _env->platform->new_async_event(_env->platform);
}

static NetworkChannelState _get_state(TcpIpChannel *self) { return self->state; }

static lf_ret_t _TcpIpChannel_reset_socket(TcpIpChannel *self) {
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

  _update_state(self, NETWORK_CHANNEL_STATE_OPEN);
  _update_state(self, NETWORK_CHANNEL_STATE_OPEN);

  return LF_OK;
}

static void _spawn_worker_thread(TcpIpChannel *self) {
  static void _spawn_worker_thread(TcpIpChannel * self) {
    int res;
    LF_INFO(NET, "TCP/IP spawning callback thread");

    memset(&self->receive_thread_stack, 0, TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE);
    if (pthread_attr_init(&self->receive_thread_attr) != 0) {
      throw("pthread_attr_init failed");
    }
/* TODO: RIOT posix-wrappers don't have pthread_attr_setstack yet => This can be removed once RIOT 2024.10 is released
 * at the end of november */
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
                              TCP_IP_CHANNEL_RECV_THREAD_STACK_SIZE - TCP_IP_CHANNEL_RECV_THREAD_STACK_GUARD_SIZE) <
        0) {
      throw("pthread_attr_setstack failed");
    }
#endif
    res = pthread_create(&self->receive_thread, &self->receive_thread_attr, _worker_thread, self);
    res = pthread_create(&self->receive_thread, &self->receive_thread_attr, _worker_thread, self);
    if (res < 0) {
      throw("pthread_create failed");
    }
  }

  static lf_ret_t _server_bind(TcpIpChannel * self) {
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = self->protocol_family;
    serv_addr.sin_port = htons(self->port);
    static lf_ret_t _server_bind(TcpIpChannel * self) {
      struct sockaddr_in serv_addr;
      serv_addr.sin_family = self->protocol_family;
      serv_addr.sin_port = htons(self->port);

      // turn human-readable address into something the os can work with
      if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
        LF_ERR(NET, "Invalid address %s", self->host);
        return LF_INVALID_VALUE;
      }
      // turn human-readable address into something the os can work with
      if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
        LF_ERR(NET, "Invalid address %s", self->host);
        return LF_INVALID_VALUE;
      }

      // bind the socket to that address
      int ret = bind(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
      if (ret < 0) {
        LF_ERR(NET, "Could not bind to %s:%d", self->host, self->port);
        _update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
        return LF_ERR;
      }
      // bind the socket to that address
      int ret = bind(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
      if (ret < 0) {
        LF_ERR(NET, "Could not bind to %s:%d", self->host, self->port);
        _update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
        return LF_ERR;
      }

      // start listening
      if (listen(self->fd, 1) < 0) {
        LF_ERR(NET, "Could not listen to %s:%d", self->host, self->port);
        _update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
        return LF_ERR;
        // start listening
        if (listen(self->fd, 1) < 0) {
          LF_ERR(NET, "Could not listen to %s:%d", self->host, self->port);
          _update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
          return LF_ERR;
        }

        return LF_OK;
      }

      static lf_ret_t _TcpIpChannel_try_connect_server(NetworkChannel * untyped_self) {
        TcpIpChannel *self = (TcpIpChannel *)untyped_self;

        int new_socket;
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(address);

        new_socket = accept(self->fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket >= 0) {
          self->client = new_socket;
          FD_SET(new_socket, &self->set);
          validate(self->receive_thread == 0);
          _spawn_worker_thread(self);
          _update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
          _spawn_worker_thread(self);
          _update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
          return LF_OK;
        } else {
          if (errno == EWOULDBLOCK || errno == EAGAIN) {
            LF_DEBUG(NET, "Accept failed. Try again. (errno=%d)", errno);
            return LF_OK;
            return LF_OK;
          } else {
            LF_ERR(NET, "Accept failed. Unknown errno=%d", errno);
            throw("Accept failed");
            return LF_ERR;
          }
        }
      }

      static lf_ret_t _TcpIpChannel_check_if_socket_is_writable(int fd) {
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
          }
        } else if (ret == 0) {
          // Timeout
          LF_DEBUG(NET, "Select timed out");
        } else {
          LF_DEBUG(NET, "Select failed with errno=%d", errno);
        }

        return LF_ERR;

        return LF_ERR;
      }

      static lf_ret_t _TcpIpChannel_check_socket_error(int fd) {
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

      static lf_ret_t _TcpIpChannel_try_connect_client(NetworkChannel * untyped_self) {
        TcpIpChannel *self = (TcpIpChannel *)untyped_self;
        lf_ret_t lf_ret;

        if (_get_state(self) == NETWORK_CHANNEL_STATE_OPEN) {
          if (_get_state(self) == NETWORK_CHANNEL_STATE_OPEN) {
            // First time trying to connect
            struct sockaddr_in serv_addr;

            serv_addr.sin_family = self->protocol_family;
            serv_addr.sin_port = htons(self->port);

            if (inet_pton(self->protocol_family, self->host, &serv_addr.sin_addr) <= 0) {
              return LF_INVALID_VALUE;
            }

            int ret = connect(self->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            if (ret == 0) {
              _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
              return LF_OK;
            } else {
              if (errno == EINPROGRESS) {
                _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS);
                LF_DEBUG(NET, "Connection in progress!");
                return LF_OK;
                return LF_OK;
              } else {
                LF_ERR(NET, "Connect failed errno=%d", errno);
                _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
                _TcpIpChannel_reset_socket(self);
                return LF_ERR;
              }
            }
          } else if (_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS) {
          } else if (_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS) {
            // Connection is in progress
            lf_ret = _TcpIpChannel_check_if_socket_is_writable(self->fd);
            if (lf_ret == LF_OK) {
              LF_DEBUG(NET, "Socket is writable");
              lf_ret = _TcpIpChannel_check_socket_error(self->fd);
              if (lf_ret == LF_OK) {
                LF_DEBUG(NET, "Connection succeeded");
                _update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
                _spawn_worker_thread(self);
                _update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
                _spawn_worker_thread(self);
                return LF_OK;
              } else {
                _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
                LF_ERR(NET, "Connection failed");
                _TcpIpChannel_reset_socket(self);
                return LF_ERR;
              }
            } else {
              _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
              _TcpIpChannel_reset_socket(self);
              LF_ERR(NET, "Select failed errno=%d", errno);
              return LF_ERR;
              return LF_ERR;
            }
          } else {
            LF_ERR(NET, "try_connect_client called in invalid state %d", _get_state(self));
            LF_ERR(NET, "try_connect_client called in invalid state %d", _get_state(self));
            return LF_ERR;
          }

          return LF_ERR; // Should never reach here
        }

        static lf_ret_t TcpIpChannel_open_connection(NetworkChannel * untyped_self) {
          static lf_ret_t TcpIpChannel_open_connection(NetworkChannel * untyped_self) {
            TcpIpChannel *self = (TcpIpChannel *)untyped_self;

            _update_state(self, NETWORK_CHANNEL_STATE_OPEN);

            return LF_OK;

            _update_state(self, NETWORK_CHANNEL_STATE_OPEN);

            return LF_OK;
          }

          static lf_ret_t TcpIpChannel_send_blocking(NetworkChannel * untyped_self, const FederateMessage *message) {
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
                switch (errno) {
                case ETIMEDOUT:
                case ENOTCONN:
                  _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
                  return LF_ERR;
                default:
                  return LF_ERR;
                }
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

          static lf_ret_t _TcpIpChannel_receive(NetworkChannel * untyped_self, FederateMessage * return_message) {
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

            if (self->read_index > 0) {
              LF_DEBUG(NET, "Has %d bytes in read_buffer from last recv. Trying to deserialize", self->read_index);
              bytes_left = deserialize_from_protobuf(&self->output, self->read_buffer, self->read_index);
              if (bytes_left >= 0) {
                LF_DEBUG(NET, "%d bytes left after deserialize", bytes_left);
                read_more = false;
              }
            }

            while (read_more) {
              // reading from socket
              LF_DEBUG(NET, "Reading up until %d bytes from socket %d", bytes_available, socket);
              ssize_t bytes_read = recv(socket, self->read_buffer + self->read_index, bytes_available, 0);
              LF_DEBUG(NET, "Read %d bytes from socket %d", bytes_read, socket);

              if (bytes_read < 0) {
                LF_ERR(NET, "[%s] Error recv from socket %d", self->server ? "server" : "client", errno);
                switch (errno) {
                case ETIMEDOUT:
                case ECONNRESET:
                case ENOTCONN:
                case ECONNABORTED:
                  _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
                  return LF_ERR;
                  break;
                }
                continue;
              } else if (bytes_read == 0) {
                // This means the connection was closed.
                LF_INFO(NET, "Connection closed");
                self->terminate = true;
                _TcpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CLOSED);
                return LF_ERR;
              }

              self->read_index += bytes_read;
              bytes_left = deserialize_from_protobuf(return_message, self->read_buffer, self->read_index);
              LF_DEBUG(NET, "%d bytes left after deserialize", bytes_left);
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

          static void TcpIpChannel_close_connection(NetworkChannel * untyped_self) {
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

          /**
           * @brief Main loop of the TcpIpChannel.
           */
          static void *_worker_thread(void *untyped_self) {
            /**
             * @brief Main loop of the TcpIpChannel.
             */
            static void *_worker_thread(void *untyped_self) {
              LF_INFO(NET, "Starting TCP/IP receive thread");
              TcpIpChannel *self = untyped_self;
              lf_ret_t ret;

              // set terminate to false so the loop runs
              self->terminate = false;

              if (self->server) {
                _server_bind(self);
              }

              if (self->server) {
                _server_bind(self);
              }

              while (!self->terminate) {
                switch (_get_state(self)) {
                case NETWORK_CHANNEL_STATE_OPEN: {
                  /* try to connect */
                  if (self->server) {
                    _try_connect_server(untyped_self);
                  } else {
                    _try_connect_client(untyped_self);
                  }
                } break;

                case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS: {
                  _env->platform->wait_for(_env->platform, self->super.expected_try_connect_duration);
                } break;

                case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
                case NETWORK_CHANNEL_STATE_CONNECTION_FAILED: {
                  _env->platform->wait_for(_env->platform, self->super.expected_try_connect_duration);
                  _reset_socket(self);
                } break;

                case NETWORK_CHANNEL_STATE_CONNECTED: {
                  ret = _receive(untyped_self, &self->output);
                  if (ret == LF_OK) {
                    validate(self->receive_callback);
                    self->receive_callback(self->federated_connection, &self->output);
                  } else {
                    LF_ERR(NET, "Error receiving message %d", ret);
                  }
                } break;

                case NETWORK_CHANNEL_STATE_UNINITIALIZED:
                case NETWORK_CHANNEL_STATE_CLOSED:
                  switch (_get_state(self)) {
                  case NETWORK_CHANNEL_STATE_OPEN: {
                    /* try to connect */
                    if (self->server) {
                      _try_connect_server(untyped_self);
                    } else {
                      _try_connect_client(untyped_self);
                    }
                  } break;

                  case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS: {
                    _env->platform->wait_for(_env->platform, self->super.expected_try_connect_duration);
                  } break;

                  case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
                  case NETWORK_CHANNEL_STATE_CONNECTION_FAILED: {
                    _env->platform->wait_for(_env->platform, self->super.expected_try_connect_duration);
                    _reset_socket(self);
                  } break;

                  case NETWORK_CHANNEL_STATE_CONNECTED: {
                    ret = _receive(untyped_self, &self->output);
                    if (ret == LF_OK) {
                      validate(self->receive_callback);
                      self->receive_callback(self->federated_connection, &self->output);
                    } else {
                      LF_ERR(NET, "Error receiving message %d", ret);
                    }
                  } break;

                  case NETWORK_CHANNEL_STATE_UNINITIALIZED:
                  case NETWORK_CHANNEL_STATE_CLOSED:
                    break;
                  }
                }

                return NULL;
              }

              static void TcpIpChannel_register_receive_callback(
                  NetworkChannel * untyped_self,
                  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *msg),
                  FederatedConnectionBundle *conn) {
                LF_DEBUG(NET, "Registering receive callback");
                TcpIpChannel *self = (TcpIpChannel *)untyped_self;
                self->receive_callback = receive_callback;
                self->federated_connection = conn;
              }

              static void TcpIpChannel_free(NetworkChannel * untyped_self) {
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

              static NetworkChannelState TcpIpChannel_get_connection_state(NetworkChannel * untyped_self) {
                TcpIpChannel *self = (TcpIpChannel *)untyped_self;
                return _get_state(self);
                return _get_state(self);
              }

              void TcpIpChannel_ctor(TcpIpChannel * self, Environment * env, const char *host, unsigned short port,
                                     int protocol_family, bool server) {
                assert(self != NULL);
                assert(env != NULL);
                assert(host != NULL);

                // Initialize global coap server if not already done
                if (!_is_globals_initialized) {
                  _is_globals_initialized = true;

                  // Set environment
                  _env = env;
                }
                void TcpIpChannel_ctor(TcpIpChannel * self, Environment * env, const char *host, unsigned short port,
                                       int protocol_family, bool server) {
                  assert(self != NULL);
                  assert(env != NULL);
                  assert(host != NULL);

                  // Initialize global coap server if not already done
                  if (!_is_globals_initialized) {
                    _is_globals_initialized = true;

                    // Set environment
                    _env = env;
                  }

                  self->server = server;
                  self->terminate = true;
                  self->protocol_family = protocol_family;
                  self->host = host;
                  self->port = port;
                  self->read_index = 0;
                  self->client = 0;
                  self->fd = 0;
                  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;

                  self->super.get_connection_state = TcpIpChannel_get_connection_state;
                  self->super.open_connection = TcpIpChannel_open_connection;
                  self->super.close_connection = TcpIpChannel_close_connection;
                  self->super.send_blocking = TcpIpChannel_send_blocking;
                  self->super.register_receive_callback = TcpIpChannel_register_receive_callback;
                  self->super.free = TcpIpChannel_free;
                  self->super.expected_try_connect_duration = MSEC(10); // Needed for Zephyr
                  self->super.type = NETWORK_CHANNEL_TYPE_TCP_IP;
                  self->receive_callback = NULL;
                  self->federated_connection = NULL;
                  self->receive_thread = 0;

                  _TcpIpChannel_reset_socket(self);
                }
