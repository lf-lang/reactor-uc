#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <zephyr/net/net_ip.h>

#define PORT_NUM 8901

typedef struct {
  char msg[32];
} msg_t;

// Reactor Sender
typedef struct {
  Timer super;
  Reaction *effects[1];
} Timer1;

typedef struct {
  Reaction super;
} Reaction1;

typedef struct {
  Output super;
  Reaction *sources[1];
  msg_t value;
} Out;

typedef struct {
  Reactor super;
  Reaction1 reaction;
  Timer1 timer;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

void timer_handler(Reaction *_self) {
  Sender *self = (Sender *)_self->parent;
  Environment *env = self->super.env;
  Out *out = &self->out;

  printf("Timer triggered @ %" PRId64 "\n", env->get_elapsed_logical_time(env));
  msg_t val;
  strcpy(val.msg, "Hello From Sender");
  lf_set(out, val);
}

void Reaction1_ctor(Reaction1 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, timer_handler, NULL, 0, 0);
}

void Out_ctor(Out *self, Sender *parent) {
  self->sources[0] = &parent->reaction.super;
  Output_ctor(&self->super, &parent->super, self->sources, 1);
}

void Sender_ctor(Sender *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction1_ctor(&self->reaction, &self->super);
  Timer_ctor(&self->timer.super, &self->super, 0, SEC(1), self->timer.effects, 1);
  Out_ctor(&self->out, self);
  TIMER_REGISTER_EFFECT(self->timer, self->reaction);

  // Register reaction as a source for out
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

typedef struct {
  FederatedOutputConnection super;
  msg_t buffer[1];
} ConnSender;

void ConnSender_ctor(ConnSender *self, Reactor *parent, FederatedConnectionBundle *bundle) {
  FederatedOutputConnection_ctor(&self->super, parent, bundle, 0, (void *)&self->buffer[0], sizeof(self->buffer[0]));
}

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel chan;
  ConnSender conn;
  FederatedOutputConnection *output[1];
} SenderRecvBundle;

void SenderRecvBundle_ctor(SenderRecvBundle *self, Reactor *parent) {
  TcpIpChannel_ctor(&self->chan, "192.168.1.100", PORT_NUM, AF_INET);
  ConnSender_ctor(&self->conn, parent, &self->super);
  self->output[0] = &self->conn.super;

  NetworkChannel *chan = &self->chan.super;
  int ret = chan->bind(chan);
  validate(ret == LF_OK);
  printf("Sender: Bound\n");

  // accept one connection
  bool new_connection = chan->accept(chan);
  validate(new_connection);
  printf("Sender: Accepted\n");

  FederatedConnectionBundle_ctor(&self->super, parent, &self->chan.super, NULL, 0,
                                 (FederatedOutputConnection **)&self->output, 1);
}

// Reactor main
struct MainSender {
  Reactor super;
  Sender sender;
  SenderRecvBundle bundle;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
};

void MainSender_ctor(struct MainSender *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env);
  Reactor_ctor(&self->super, "MainSender", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
  SenderRecvBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_UPSTREAM(self->bundle.conn, self->sender.out);

  self->_bundles[0] = &self->bundle.super;
}

Environment env_send;
struct MainSender sender;
int main() {
  printf("Sender starting!\n");
  Environment_ctor(&env_send, (Reactor *)&sender);
  MainSender_ctor(&sender, &env_send);
  env_send.net_bundles_size = 1;
  env_send.net_bundles = (FederatedConnectionBundle **)&sender._bundles;
  env_send.assemble(&env_send);
  env_send.start(&env_send);
  return 0;
}

void lf_exit(void) { Environment_free(&env_send); }
