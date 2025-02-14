#ifndef REACTOR_UC_NETWORK_CHANNEL_H
#define REACTOR_UC_NETWORK_CHANNEL_H

#include <nanopb/pb.h>

#include "proto/message.pb.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"
#include "reactor-uc/federated.h"
#include <unistd.h>

/**
 * @brief The current state of the connection.
 * NETWORK_CHANNEL_STATE_UNINITIALIZED if the connection has not been initialized yet,
 * NETWORK_CHANNEL_STATE_OPEN if the connection is open and waiting for try_connect to be called,
 * NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS if try_connect has been called but it is not yet connected,
 * NETWORK_CHANNEL_STATE_CONNECTION_FAILED if the connection failed,
 * NETWORK_CHANNEL_STATE_CONNECTED if the channel is successfully connected to another federate,
 * NETWORK_CHANNEL_STATE_LOST_CONNECTION if the connection was unexpectedly closed,
 * NETWORK_CHANNEL_STATE_CLOSED if the connection was manually closed.
 */
typedef enum {
  NETWORK_CHANNEL_STATE_UNINITIALIZED,
  NETWORK_CHANNEL_STATE_OPEN,
  NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS,
  NETWORK_CHANNEL_STATE_CONNECTION_FAILED,
  NETWORK_CHANNEL_STATE_CONNECTED,
  NETWORK_CHANNEL_STATE_LOST_CONNECTION,
  NETWORK_CHANNEL_STATE_CLOSED,
} NetworkChannelState;

typedef enum {
  NETWORK_CHANNEL_TYPE_TCP_IP,
  NETWORK_CHANNEL_TYPE_COAP_UDP_IP,
  NETWORK_CHANNEL_TYPE_UART
} NetworkChannelType;

typedef enum {
  NETWORK_CHANNEL_MODE_ASYNC,
  NETWORK_CHANNEL_MODE_POLLED,
} NetworkChannelMode;

char *NetworkChannel_state_to_string(NetworkChannelState state);

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct NetworkChannel NetworkChannel;
typedef struct PolledNetworkChannel PolledNetworkChannel;
typedef struct AsyncNetworkChannel AsyncNetworkChannel;
typedef struct EncryptionLayer EncryptionLayer;

struct NetworkChannel {
  /**
   * @brief Specifies if this NetworkChannel is a aync or async channel
   */
  NetworkChannelMode mode;

  /**
   * @brief Expected time until a connection is established after calling @p open_connection.
   */
  interval_t expected_connect_duration;

  /**
   * @brief Type of the network channel to differentiate between different implementations such as TcpIp or CoapUdpIp.
   */
  NetworkChannelType type;

  /**
   * @brief Get the current state of the connection.
   * @return true if the channel is connected, false if the channel is not connected
   */
  bool (*is_connected)(NetworkChannel *self);

  /**
   * @brief Has the network channel ever been connected to its peer?
   * This is needed because we currently require an initial connection
   * to be established to all peers before a federate can start.
   */
  bool (*was_ever_connected)(NetworkChannel *self);

  /**
   * @brief Opens the connection to the corresponding NetworkChannel on another federate (non-blocking).
   * The channel is not connected unless @p is_connected returns true.
   * @return LF_OK if channel opened without error, LF_ERR if the channel is configured incorrectly or the connection
   * open operation fails.
   */
  lf_ret_t (*open_connection)(NetworkChannel *self);

  /**
   * @brief Closes the connection to the corresponding NetworkChannel on another federate.
   */
  void (*close_connection)(NetworkChannel *self);

  /**
   * @brief Sends a FederateMessage (blocking).
   * @return LF_OK if message is sent successfully, LF_ERR if sending message failed.
   */
  lf_ret_t (*send_blocking)(NetworkChannel *self, const char *message, size_t message_size);

  /**
   * @brief Register async callback for handling incoming messages from another federate.
   */
  void (*register_receive_callback)(NetworkChannel *self, EncryptionLayer *layer,
                                    void (*receive_callback)(EncryptionLayer *layer, const char *buffer, ssize_t size));

  /**
   * @brief Free up NetworkChannel, join threads etc.
   */
  void (*free)(NetworkChannel *self);
};

struct PolledNetworkChannel {
  NetworkChannel super;

  /**
   * @brief Polls for new data and calls the callback handler if a message is successfully decoded
   */
  void (*poll)(PolledNetworkChannel *self);
};

struct AsyncNetworkChannel {
  NetworkChannel super;
};

#if defined(PLATFORM_POSIX)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#include "platform/posix/tcp_ip_channel.h"
#endif

#elif defined(PLATFORM_ZEPHYR)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#include "platform/posix/tcp_ip_channel.h"
#endif

#elif defined(PLATFORM_RIOT)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#include "platform/posix/tcp_ip_channel.h"
#endif
#ifdef NETWORK_CHANNEL_COAP
#include "platform/riot/coap_udp_ip_channel.h"
#endif
#ifdef NETWORK_CHANNEL_UART
#include "platform/riot/uart_channel.h"
#include "network_channel/uart_channel.h"
#endif

#elif defined(PLATFORM_PICO)
#ifdef NETWORK_CHANNEL_UART
#include "platform/pico/uart_channel.h"
#endif
#ifdef NETWORK_CHANNEL_TCP_POSIX
#error "NETWORK_POSIX_TCP not supported on PICO"
#endif
#ifdef NETWORK_CHANNEL_UART
#include "platform/pico/uart_channel.h"
#include "network_channel/uart_channel.h"
#endif
#elif defined(PLATFORM_FLEXPRET)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#error "NETWORK_POSIX_TCP not supported on FlexPRET"
#endif

#else
#error "Platform not supported"
#endif

#endif // REACTOR_UC_NETWORK_CHANNEL_H
