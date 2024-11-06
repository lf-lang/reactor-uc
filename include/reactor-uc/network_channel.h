#ifndef REACTOR_UC_NETWORK_CHANNEL_H
#define REACTOR_UC_NETWORK_CHANNEL_H

#include <nanopb/pb.h>
#include <pthread.h>
#include <sys/select.h>

#include "proto/message.pb.h"
#include "reactor-uc/error.h"
#include "reactor-uc/federated.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct NetworkChannel NetworkChannel;

struct NetworkChannel {
  size_t dest_channel_id; // So that we can "address" one of several NetworkChannel's at the other end.

  lf_ret_t (*open_connection)(NetworkChannel *self);

  /** Try to connect to corresponding NetworkChannel on another federate.
   * @return LF_OK if connection is established, LF_IN_PROGRESS if connection is in progress, LF_TRY_AGAIN if connection
   * failed and should be retried, LF_ERR if connection failed and should not be retried.
   */
  lf_ret_t (*try_connect)(NetworkChannel *self);
  void (*close_connection)(NetworkChannel *self);
  lf_ret_t (*send_blocking)(NetworkChannel *self, const FederateMessage *message);
  void (*register_receive_callback)(NetworkChannel *self,
                                    void (*receive_callback)(FederatedConnectionBundle *conn,
                                                             const FederateMessage *message),
                                    FederatedConnectionBundle *conn);
  void (*free)(NetworkChannel *self);
};

#endif // REACTOR_UC_NETWORK_CHANNEL_H
