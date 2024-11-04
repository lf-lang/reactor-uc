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
  memcpy(msg->msg, msg_buf + sizeof(msg->size), sizeof(msg->size));

  return LF_OK;
}

DEFINE_REACTION_STRUCT(Receiver, 0, 1)
DEFINE_INPUT_STRUCT(In, 1, msg_t, 0)
DEFINE_INPUT_CTOR(In, 1, msg_t, 0)

typedef struct {
  Reactor super;
  Receiver_Reaction0 reaction;
  In inp;
  int cnt;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver;

DEFINE_REACTION_BODY(Receiver, 0) {
  Receiver *self = (Receiver *)_self->parent;
  Environment *env = self->super.env;
  In *inp = &self->inp;
  printf("Input triggered @ %" PRId64 " with %s\n", env->get_elapsed_logical_time(env), inp->value.msg);
}
DEFINE_REACTION_CTOR(Receiver, 0)

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->inp;
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Receiver_Reaction0_ctor(&self->reaction, &self->super);
  In_ctor(&self->inp, &self->super);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(self->inp, self->reaction);
}

DEFINE_FEDERATED_INPUT_CONNECTION(ConnRecv, 1, msg_t, 5, MSEC(100), false)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  ConnRecv conn;
  FederatedInputConnection *inputs[1];
  deserialize_hook deserialize_hooks[1];
} RecvSenderBundle;

void RecvSenderBundle_ctor(RecvSenderBundle *self, Reactor *parent) {
  ConnRecv_ctor(&self->conn, parent);
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET, false);
  self->inputs[0] = &self->conn.super;

  NetworkChannel *channel = (NetworkChannel *)&self->channel;
  channel->open_connection(channel);

  lf_ret_t ret;
  do {
    ret = channel->try_connect(channel);
  } while (ret != LF_OK);
  validate(ret == LF_OK);
  printf("Recv: Connected\n");
  self->deserialize_hooks[0] = deserialize_msg_t;

  FederatedConnectionBundle_ctor(&self->super, parent, &self->channel.super, (FederatedInputConnection **)&self->inputs,
                                 self->deserialize_hooks, 1, NULL, NULL, 0);
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

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn, self->receiver.inp);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
}

ENTRY_POINT_FEDERATED(MainRecv, SEC(1), true, true, 1, false)

int main() {
  lf_start();
  return 0;
}
