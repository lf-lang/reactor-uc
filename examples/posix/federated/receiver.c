#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/platform/posix/no_encryption.h"
#include "reactor-uc/reactor-uc.h"

#include <pthread.h>
#include <sys/socket.h>

#define PORT_NUM 8901

typedef struct {
  int size;
  char msg[512];
} msg_t;

lf_ret_t deserialize_msg_t(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  msg_t *msg = user_struct;
  memcpy(&msg->size, msg_buf, sizeof(msg->size));
  memcpy(msg->msg, msg_buf + sizeof(msg->size), msg->size);

  return LF_OK;
}

LF_DEFINE_REACTION_STRUCT(Receiver, r, 0);
LF_DEFINE_REACTION_CTOR(Receiver, r, 0);
LF_DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, msg_t, 0);
LF_DEFINE_INPUT_CTOR(Receiver, in, 1, 0, msg_t, 0);

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Receiver, r);
  LF_PORT_INSTANCE(Receiver, in, 1);
  int cnt;
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
} Receiver;

LF_DEFINE_REACTION_BODY(Receiver, r) {
  LF_SCOPE_SELF(Receiver);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Receiver, in);
  printf("Input triggered @ %" PRId64 " with %s size %d\n", env->get_elapsed_logical_time(env), in->value.msg,
         in->value.size);
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Receiver);
  LF_INITIALIZE_REACTION(Receiver, r);
  LF_INITIALIZE_INPUT(Receiver, in, 1, in_external);

  // Register reaction as an effect of in
  LF_PORT_REGISTER_EFFECT(self->in, self->r, 1);
}

LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(Receiver, in, msg_t, 5);
LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(Receiver, in, msg_t, 5, MSEC(100), false);

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  NoEncryptionLayer encryption_layer;
  LF_FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(1, 0)
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Receiver, Sender);

LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET, false);
  NoEncryptionLayer_ctor(&self->encryption_layer, (NetworkChannel*)&self->channel);
  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  LF_FEDERATE_BOOKKEEPING_INSTANCES(1);
  LF_CHILD_INPUT_SOURCES(receiver, in, 1, 1, 0);
} MainRecv;

LF_REACTOR_CTOR_SIGNATURE(MainRecv) {
  LF_REACTOR_CTOR(MainRecv);
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, _receiver_in_args[i]);
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  lf_connect_federated_input(&self->Receiver_Sender_bundle.inputs[0]->super, &self->receiver->in[0].super);
}

LF_ENTRY_POINT_FEDERATED(MainRecv, SEC(1), true, true, 1, false)

int main() {
  lf_start();
  return 0;
}
