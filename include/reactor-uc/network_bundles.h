#ifndef REACTOR_UC_NETWORK_BUNDLES_H
#define REACTOR_UC_NETWORK_BUNDLES_H

#include <nanopb/pb.h>
#include <pthread.h>
#include <sys/select.h>

#include "proto/message.pb.h"
#include "reactor-uc/error.h"
#include "reactor-uc/federated.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct NetworkBundle NetworkBundle;

struct NetworkBundle {
  lf_ret_t (*bind)(NetworkBundle *self);
  lf_ret_t (*connect)(NetworkBundle *self);
  bool (*accept)(NetworkBundle *self);
  void (*close)(NetworkBundle *self);
  void (*change_block_state)(NetworkBundle *self, bool blocking);
  void (*register_callback)(NetworkBundle *self,
                            void (*receive_callback)(FederatedConnectionBundle *conn, TaggedMessage *message),
                            FederatedConnectionBundle *conn);
  lf_ret_t (*send)(NetworkBundle *self, TaggedMessage *message);
  TaggedMessage *(*receive)(NetworkBundle *self);
};

#endif // REACTOR_UC_NETWORK_BUNDLES_H
