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
  lf_ret_t (*bind)(NetworkChannel *self);
  lf_ret_t (*connect)(NetworkChannel *self);
  bool (*accept)(NetworkChannel *self);
  void (*close)(NetworkChannel *self);
  void (*change_block_state)(NetworkChannel *self, bool blocking);
  void (*register_callback)(NetworkChannel *self,
                            void (*receive_callback)(FederatedConnectionBundle *conn, TaggedMessage *message),
                            FederatedConnectionBundle *conn);
  lf_ret_t (*send)(NetworkChannel *self, TaggedMessage *message);
  TaggedMessage *(*receive)(NetworkChannel *self);
  void (*free)(NetworkChannel *self);
};

#endif // REACTOR_UC_NETWORK_CHANNEL_H
