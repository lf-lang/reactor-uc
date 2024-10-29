#include "board.h"
#include "sys/socket.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"

#define PORT_NUM 8902

typedef struct {
  char msg[32];
} my_msg_t;

DEFINE_REACTION_STRUCT(Receiver, 0, 0)
DEFINE_INPUT_PORT_STRUCT(In, 1, my_msg_t, 0)
DEFINE_INPUT_PORT_CTOR(In, 1, my_msg_t, 0)

typedef struct {
  Reactor super;
  Receiver_Reaction0 reaction;
  In inp;
  int cnt;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver;

DEFINE_REACTION_BODY(Receiver, 0) {
  Environment *env = _self->parent->env;
  LED0_TOGGLE;
  printf("Reaction triggered @ %" PRId64 " , %" PRId64 ")\n", env->get_elapsed_logical_time(env),
         env->get_physical_time(env));
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

DEFINE_FEDERATED_INPUT_CONNECTION(ConnRecv, 1, my_msg_t, 5, 0, false)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel chan;
  ConnRecv conn;
  FederatedInputConnection *inputs[1];
  deserialize_hook deserialize_hooks[1];
} RecvSenderBundle;

void RecvSenderBundle_ctor(RecvSenderBundle *self, Reactor *parent) {
  ConnRecv_ctor(&self->conn, parent);
  TcpIpChannel_ctor(&self->chan, "192.168.1.100", PORT_NUM, AF_INET, false);
  self->inputs[0] = &self->conn.super;

  NetworkChannel *chan = &self->chan.super;
  chan->open_connection(chan);

  lf_ret_t ret;
  LF_DEBUG(ENV, "Recv: Connecting");
  do {
    ret = chan->try_connect(chan);
  } while (ret != LF_OK);
  LF_DEBUG(ENV, "Recv: Connected");

  FederatedConnectionBundle_ctor(&self->super, parent, &self->chan.super, (FederatedInputConnection **)&self->inputs,
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
  env->wait_until(env, env->get_physical_time(env) +
                           MSEC(400)); // FIXME: This is a hack until we have proper connection setup
  self->_children[0] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  RecvSenderBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn, self->receiver.inp);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);

  self->_bundles[0] = &self->bundle.super;
}

ENTRY_POINT_FEDERATED(MainRecv, FOREVER, true, true, 1, false)

int main(void) { lf_start(); }