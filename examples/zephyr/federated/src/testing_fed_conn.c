#include "reactor-uc/platform/zephyr/tcp_ip_channel.h"
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

// Reactor Receiver
typedef struct {
  Reaction super;
} Reaction2;

typedef struct {
  Input super;
  msg_t buffer[1];
  Reaction *effects[1];
} In;

typedef struct {
  Reactor super;
  Reaction2 reaction;
  In inp;
  int cnt;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver;

void In_ctor(In *self, Receiver *parent) {
  Input_ctor(&self->super, &parent->super, self->effects, 1, self->buffer, sizeof(self->buffer[0]));
}

void input_handler(Reaction *_self) {
  Receiver *self = (Receiver *)_self->parent;
  Environment *env = self->super.env;
  In *inp = &self->inp;

  printf("Input triggered @ %" PRId64 " with %s\n", env->get_elapsed_logical_time(env), lf_get(inp).msg);
}

void Reaction2_ctor(Reaction2 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, input_handler, NULL, 0, 0);
}

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->inp;
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction2_ctor(&self->reaction, &self->super);
  In_ctor(&self->inp, self);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(self->inp, self->reaction);
}

typedef struct {
  FederatedOutputConnection super;
  msg_t buffer[1];
} ConnSender;

void ConnSender_ctor(ConnSender *self, Reactor *parent, FederatedConnectionBundle *bundle, Port *upstream) {
  FederatedOutputConnection_ctor(&self->super, parent, bundle, 0, upstream, &self->buffer[0], sizeof(self->buffer[0]));
}

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel chan;
  ConnSender conn;
  FederatedOutputConnection *output[1];
} SenderRecvBundle;

void SenderRecvConn_ctor(SenderRecvBundle *self, Sender *parent) {
  TcpIpChannel_ctor(&self->chan, "127.0.0.1", PORT_NUM, AF_INET);
  ConnSender_ctor(&self->conn, &parent->super, &self->super, &parent->out.super.super);
  self->output[0] = &self->conn.super;

  NetworkChannel *chan = &self->chan.super;
  int ret = chan->bind(chan);
  validate(ret == LF_OK);
  printf("Sender: Bound\n");

  // accept one connection
  bool new_connection = chan->accept(chan);
  validate(new_connection);
  printf("Sender: Accepted\n");

  FederatedConnectionBundle_ctor(&self->super, &parent->super, &self->chan.super, NULL, 0,
                                 (FederatedOutputConnection **)&self->output, 1);
}

typedef struct {
  FederatedInputConnection super;
  msg_t buffer[5];
  Input *downstreams[1];
} ConnRecv;

void ConnRecv_ctor(ConnRecv *self, Reactor *parent) {
  FederatedInputConnection_ctor(&self->super, parent, MSEC(100), false, (Port **)&self->downstreams, 1,
                                &self->buffer[0], sizeof(self->buffer[0]), 5);
}

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel chan;
  ConnRecv conn;
  FederatedInputConnection *inputs[1];
} RecvSenderBundle;

void RecvSenderBundle_ctor(RecvSenderBundle *self, Reactor *parent) {
  ConnRecv_ctor(&self->conn, parent);
  TcpIpChannel_ctor(&self->chan, "127.0.0.1", PORT_NUM, AF_INET);
  self->inputs[0] = &self->conn.super;

  NetworkChannel *chan = &self->chan.super;

  lf_ret_t ret;
  do {
    ret = chan->connect(chan);
  } while (ret != LF_OK);
  printf("Recv: Connected\n");

  FederatedConnectionBundle_ctor(&self->super, parent, &self->chan.super, (FederatedInputConnection **)&self->inputs, 1,
                                 NULL, 0);
}

// Reactor main
struct MainSender {
  Reactor super;
  Sender sender;
  SenderRecvBundle bundle;
  FederatedConnectionBundle *net_bundles[1];
  Reactor *_children[1];
};

struct MainRecv {
  Reactor super;
  Receiver receiver;
  RecvSenderBundle bundle;
  FederatedConnectionBundle *net_bundles[1];
  Reactor *_children[1];
};

void MainSender_ctor(struct MainSender *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env);

  SenderRecvConn_ctor(&self->bundle, &self->sender);
  Reactor_ctor(&self->super, "MainSender", env, NULL, self->_children, 1, NULL, 0, NULL, 0);

  self->net_bundles[0] = &self->bundle.super;
}

void MainRecv_ctor(struct MainRecv *self, Environment *env) {
  self->_children[0] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  RecvSenderBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn, self->receiver.inp);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);

  self->net_bundles[0] = &self->bundle.super;
}

Environment env_send;
struct MainSender sender;
void *main_sender(void *unused, void *unused2, void *unused3) {
  (void)unused;
  (void)unused2;
  (void)unused3;
  printf("Sender starting!\n");
  Environment_ctor(&env_send, (Reactor *)&sender);
  MainSender_ctor(&sender, &env_send);
  env_send.bundles_size = 1;
  env_send.bundles = (FederatedConnectionBundle **)&sender.net_bundles;
  env_send.assemble(&env_send);
  env_send.start(&env_send);
  return NULL;
}

Environment env_recv;
struct MainRecv receiver;
void *main_recv(void *unused, void *unused2, void *unused3) {
  (void)unused;
  (void)unused2;
  (void)unused3;
  printf("Receiver starting!\n");
  Environment_ctor(&env_recv, (Reactor *)&receiver);
  env_recv.platform->enter_critical_section(env_recv.platform);
  MainRecv_ctor(&receiver, &env_recv);
  env_recv.keep_alive = true;
  env_recv.has_async_events = true;
  env_recv.bundles_size = 1;
  env_recv.bundles = (FederatedConnectionBundle **)&receiver.net_bundles;
  env_recv.assemble(&env_recv);
  env_recv.platform->leave_critical_section(env_recv.platform);
  env_recv.start(&env_recv);
  return NULL;
}

void lf_exit(void) {
  Environment_free(&env_send);
  Environment_free(&env_recv);
}

K_THREAD_DEFINE(t1, 4096, main_sender, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(t2, 4096, main_recv, NULL, NULL, NULL, 5, 0, 0);

int main() { return 0; }
