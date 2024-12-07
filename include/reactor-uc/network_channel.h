#ifndef REACTOR_UC_NETWORK_CHANNEL_H
#define REACTOR_UC_NETWORK_CHANNEL_H

#include <nanopb/pb.h>

#include "proto/message.pb.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"
#include "reactor-uc/federated.h"

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
} NetworkChannelType;

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct NetworkChannel NetworkChannel;

struct NetworkChannel {
  /**
   * @brief Expected time until a connection is established after calling @p try_connect.
   */
  interval_t expected_try_connect_duration;

  /**
   * @brief Type of the network channel to differentiate between different implementations such as TcpIp or CoapUdpIp.
   */
  NetworkChannelType type;

  /**
   * @brief Get the current state of the connection.
   * @return NETWORK_CHANNEL_STATE_UNINITIALIZED if the connection has not been initialized yet,
   * NETWORK_CHANNEL_STATE_OPEN if the connection is open and waiting for try_connect to be called,
   * NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS if try_connect has been called but it is not yet connected,
   * NETWORK_CHANNEL_STATE_CONNECTION_FAILED if the connection failed,
   * NETWORK_CHANNEL_STATE_CONNECTED if the channel is successfully connected to another federate,
   * NETWORK_CHANNEL_STATE_LOST_CONNECTION if the connection was unexpectedly closed,
   * NETWORK_CHANNEL_STATE_CLOSED if the connection was manually closed.
   */
  NetworkChannelState (*get_connection_state)(NetworkChannel *self);

  /**
   * @brief Opens the connection to the corresponding NetworkChannel on another federate (non-blocking).
   * The channel is not connected unless @p get_connection_state returns with NETWORK_CHANNEL_STATE_CONNECTED.
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
  lf_ret_t (*send_blocking)(NetworkChannel *self, const FederateMessage *message);

  /**
   * @brief Register async callback for handling incoming messages from another federate.
   */
  void (*register_receive_callback)(NetworkChannel *self,
                                    void (*receive_callback)(FederatedConnectionBundle *conn,
                                                             const FederateMessage *message),
                                    FederatedConnectionBundle *conn);

  /**
   * @brief Free up NetworkChannel, join threads etc.
   */
  void (*free)(NetworkChannel *self);
};

#endif // REACTOR_UC_NETWORK_CHANNEL_H
