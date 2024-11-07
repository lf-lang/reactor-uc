#ifndef REACTOR_UC_NETWORK_CHANNEL_H
#define REACTOR_UC_NETWORK_CHANNEL_H

#include <nanopb/pb.h>
#include <pthread.h>
#include <sys/select.h>

#include "proto/message.pb.h"
#include "reactor-uc/error.h"
#include "reactor-uc/federated.h"

typedef enum {
  NETWORK_CHANNEL_STATE_UNINITIALIZED,
  NETWORK_CHANNEL_STATE_OPEN,
  NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS,
  NETWORK_CHANNEL_STATE_CONNECTED,
  NETWORK_CHANNEL_STATE_DISCONNECTED,
  NETWORK_CHANNEL_STATE_LOST_CONNECTION,
} NetworkChannelState;

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct NetworkChannel NetworkChannel;

struct NetworkChannel {
  /**
   * @brief Used to identify this NetworkChannel among other NetworkChannels at the other federate.
   */
  size_t dest_channel_id;

  /**
   * @brief Expected time until a connection is established after calling @p try_connect.
   */
  interval_t expected_try_connect_duration;

  /**
   * @brief Get the current state of the connection.
   * @return NETWORK_CHANNEL_STATE_UNINITIALIZED if the connection has not been initialized yet,
   * NETWORK_CHANNEL_STATE_OPEN if the connection is open and waiting for try_connect to be called,
   * NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS if try_connect has been called but it is not yet connected,
   * NETWORK_CHANNEL_STATE_CONNECTED if the channel is successfully connected to another federate,
   * NETWORK_CHANNEL_STATE_DISCONNECTED if the connection was manually closed,
   * NETWORK_CHANNEL_STATE_LOST_CONNECTION if the connection was unexpectedly closed.
   */
  NetworkChannelState (*get_connection_state)(NetworkChannel *self);

  /**
   * @brief Opens the connection to the corresponding NetworkChannel on another federate (non-blocking).
   * For client-server channels this usually is implemented as the "bind" call on the server side.
   * @return LF_OK if connection is opened, LF_INVALID_VALUE if the channel is configured incorrectly,
   * LF_NETWORK_SETUP_FAILED if the connection open operation fails.
   */
  lf_ret_t (*open_connection)(NetworkChannel *self);

  /**
   * @brief Try to connect to corresponding NetworkChannel on another federate (non-blocking).
   * @return LF_OK if connection is established, LF_IN_PROGRESS if connection is in progress, LF_TRY_AGAIN if connection
   * failed and should be retried, LF_ERR if connection failed and should not be retried.
   */
  lf_ret_t (*try_connect)(NetworkChannel *self);

  /**
   * @brief Try to reconnect to corresponding NetworkChannel after the connection broke of (non-blocking).
   * @return LF_OK if connection is established, LF_IN_PROGRESS if connection is in progress, LF_TRY_AGAIN if connection
   * failed and should be retried, LF_ERR if connection failed and should not be retried.
   */
  lf_ret_t (*try_reconnect)(NetworkChannel *self);

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
