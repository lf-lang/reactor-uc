#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>

#define PORT_NUM 8901

typedef struct {
  int size;
  char msg[512];
} msg_t;

size_t serialize_msg_t(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  const msg_t *msg = user_struct;

  memcpy(msg_buf, &msg->size, sizeof(msg->size));
  memcpy(msg_buf + sizeof(msg->size), msg->msg, sizeof(msg->size));

  return sizeof(msg->size) + msg->size;
}

DEFINE_TIMER_STRUCT(Timer1, 1)
DEFINE_TIMER_CTOR_FIXED(Timer1, 1, MSEC(0), SEC(1))
DEFINE_REACTION_STRUCT(Sender, 0, 1)
DEFINE_OUTPUT_PORT_STRUCT(Out, 1, 1)
DEFINE_OUTPUT_PORT_CTOR(Out, 1)

typedef struct {
  Reactor super;
  Sender_Reaction0 reaction;
  Timer1 timer;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

DEFINE_REACTION_BODY(Sender, 0) {
  Sender *self = (Sender *)_self->parent;
  Environment *env = self->super.env;
  Out *out = &self->out;

  printf("Timer triggered @ %" PRId64 "\n", env->get_elapsed_logical_time(env));
  msg_t val;
  strcpy(val.msg, "Hello From Sender");
  val.size = sizeof("Hello From Sender");
  lf_set(out, val);
}
DEFINE_REACTION_CTOR(Sender, 0)

void Sender_ctor(Sender *self, Reactor *parent, Environment *env, Connection **conn_out, size_t conn_out_num) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Sender_Reaction0_ctor(&self->reaction, &self->super);
  Timer_ctor(&self->timer.super, &self->super, 0, MSEC(100), self->timer.effects, 1);
  Out_ctor(&self->out, &self->super, conn_out, conn_out_num);
  TIMER_REGISTER_EFFECT(self->timer, self->reaction);

  // Register reaction as a source for out
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

DEFINE_REACTION_STRUCT(Receiver, 0, 1)
DEFINE_INPUT_PORT_STRUCT(In, 1, msg_t, 0)
DEFINE_INPUT_PORT_CTOR(In, 1, msg_t, 0)

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
  printf("Input triggered @ %" PRId64 " with %s size %i\n", env->get_elapsed_logical_time(env), inp->value.msg,
         inp->value.size);
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

DEFINE_FEDERATED_OUTPUT_CONNECTION(ConnSender, msg_t, 1)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  ConnSender conn;
  FederatedOutputConnection *output[1];
  serialize_hook serialize_hooks[1];
} SenderRecvBundle;

void SenderRecvConn_ctor(SenderRecvBundle *self, Sender *parent) {
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET);
  ConnSender_ctor(&self->conn, &parent->super, &self->super);
  self->output[0] = &self->conn.super;

  NetworkChannel *channel = (NetworkChannel *)&self->channel;
  int ret = channel->bind(channel);
  if (ret != LF_OK) {
    printf("bind failed with %d\n", errno);
    exit(1);
  }
  validate(ret == LF_OK);
  printf("Sender: Bound\n");

  // accept one connection
  bool new_connection = channel->accept(channel);
  validate(new_connection);
  printf("Sender: Accepted\n");

  self->serialize_hooks[0] = serialize_msg_t;

  FederatedConnectionBundle_ctor(&self->super, &parent->super, &self->channel.super, NULL, NULL, 0,
                                 (FederatedOutputConnection **)&self->output, self->serialize_hooks, 1);
}

// Reactor main
typedef struct {
  Reactor super;
  Sender sender;
  SenderRecvBundle bundle;
  TcpIpChannel channel;
  ConnSender conn;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
  Connection *_conn_sender_out[1];
} MainSender;

void MainSender_ctor(MainSender *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env, self->_conn_sender_out, 1);

  SenderRecvConn_ctor(&self->bundle, &self->sender);
  self->_bundles[0] = &self->bundle.super;
  CONN_REGISTER_UPSTREAM(self->bundle.conn, self->sender.out);
  Reactor_ctor(&self->super, "MainSender", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
}
ENTRY_POINT_FEDERATED(MainSender, SEC(1), true, false, 1)

int main() {
  lf_start();
  return 0;
}
