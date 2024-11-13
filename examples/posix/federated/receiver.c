#include "reactor-uc/platform/posix/tcp_ip_channel.h"
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

DEFINE_REACTION_STRUCT(Receiver, r, 0);
DEFINE_REACTION_CTOR(Receiver, r, 0);
DEFINE_INPUT_STRUCT(Receiver, in, 1, msg_t, 0);
DEFINE_INPUT_CTOR(Receiver, in, 1, msg_t, 0);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Receiver, r);
  PORT_INSTANCE(Receiver, in);
  int cnt;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver;

DEFINE_REACTION_BODY(Receiver, r) {
  SCOPE_SELF(Receiver);
  SCOPE_ENV();
  SCOPE_PORT(Receiver, in);
  printf("Input triggered @ %" PRId64 " with %s size %d\n", env->get_elapsed_logical_time(env), in->value.msg,
         in->value.size);
}

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _reactions_idx = 0;
  size_t _triggers_idx = 0;
  INITIALIZE_REACTION(Receiver, r);
  INITIALIZE_INPUT(Receiver, in);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(in, r);
}

DEFINE_FEDERATED_INPUT_CONNECTION(Receiver, in, msg_t, 5, MSEC(100), false);

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  FederatedInputConnection *inputs[1];
  deserialize_hook deserialize_hooks[1];
} RecvSenderBundle;

void RecvSenderBundle_ctor(RecvSenderBundle *self, Reactor *parent) {
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET, false);

  FederatedConnectionBundle_ctor(&self->super, parent, &self->channel.super, (FederatedInputConnection **)&self->inputs,
                                 self->deserialize_hooks, 1, NULL, NULL, 0);
  size_t _inputs_idx = 0;
  INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}

typedef struct {
  Reactor super;
  Receiver receiver;
  RecvSenderBundle bundle;

  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
} MainRecv;

void MainRecv_ctor(MainRecv *self, Environment *env) {
  self->_children[0] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  RecvSenderBundle_ctor(&self->bundle, &self->super);
  self->_bundles[0] = &self->bundle.super;

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn_in, self->receiver.in);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
}

ENTRY_POINT_FEDERATED(MainRecv, SEC(1), true, true, 1, false)

int main() {
  lf_start();
  return 0;
}
