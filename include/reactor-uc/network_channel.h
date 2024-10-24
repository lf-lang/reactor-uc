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

typedef int (*encode_message_hook)(const void *message, unsigned char *buffer, size_t buffer_size);
typedef int (*decode_message_hook)(void *message, const unsigned char *buffer, size_t buffer_size);

struct NetworkChannel {
  lf_ret_t (*bind)(NetworkChannel *self);
  lf_ret_t (*connect)(NetworkChannel *self);
  bool (*accept)(NetworkChannel *self);
  void (*close)(NetworkChannel *self);
  void (*register_callback)(NetworkChannel *self,
                            void (*receive_callback)(FederatedConnectionBundle *conn, void *message),
                            FederatedConnectionBundle *conn);
  lf_ret_t (*send)(NetworkChannel *self, TaggedMessage *message);
  TaggedMessage *(*receive)(NetworkChannel *self);
  void (*free)(NetworkChannel *self);

  void (*register_encode_hook)(NetworkChannel *self, encode_message_hook encode_hook);
  void (*register_decode_hook)(NetworkChannel *self, decode_message_hook decode_hook);
};

#endif // REACTOR_UC_NETWORK_CHANNEL_H
