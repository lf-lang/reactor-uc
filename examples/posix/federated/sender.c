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
  memcpy(msg_buf + sizeof(msg->size), msg->msg, msg->size);

  return sizeof(msg->size) + msg->size;
}

DEFINE_TIMER_STRUCT(Sender, t, 1)
DEFINE_TIMER_CTOR(Sender, t, 1)
DEFINE_REACTION_STRUCT(Sender, r, 1)
DEFINE_REACTION_CTOR(Sender, r, 0)
DEFINE_OUTPUT_STRUCT(Sender, out, 1)
DEFINE_OUTPUT_CTOR(Sender, out, 1)  

typedef struct {
  Reactor super;
  TIMER_INSTANCE(Sender, t);
  REACTION_INSTANCE(Sender, r);
  PORT_INSTANCE(Sender, out);
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

DEFINE_REACTION_BODY(Sender, r) {
  SCOPE_SELF(Sender);
  SCOPE_ENV();
  SCOPE_PORT(Sender, out);

  printf("Timer triggered @ %" PRId64 "\n", env->get_elapsed_logical_time(env));
  msg_t val;
  strcpy(val.msg, "Hello From Sender");
  val.size = sizeof("Hello From Sender");
  lf_set(out, val);
}

void Sender_ctor(Sender *self, Reactor *parent, Environment *env, Connection **conn_out, size_t conn_out_num) {
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _reactions_idx = 0;
  size_t _triggers_idx = 0;
  INITIALIZE_REACTION(Sender, r);
  INITIALIZE_TIMER(Sender, t, MSEC(0), SEC(1));
  INITIALIZE_OUTPUT(Sender, out, conn_out, conn_out_num);

  TIMER_REGISTER_EFFECT(t, r);
  REACTION_REGISTER_EFFECT(r, out);
  OUTPUT_REGISTER_SOURCE(out, r);
}

DEFINE_FEDERATED_OUTPUT_CONNECTION(Sender, out, msg_t, 1)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0,1);
} SenderRecvBundle;

void SenderRecvConn_ctor(SenderRecvBundle *self, Sender *parent) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET, true);
  INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);

  FederatedConnectionBundle_ctor(&self->super, &parent->super, &self->channel.super, NULL, NULL, 0,
                                 (FederatedOutputConnection **)&self->outputs, self->serialize_hooks, 1);
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
ENTRY_POINT_FEDERATED(MainSender, SEC(1), true, false, 1, true)

int main() {
  lf_start();
  return 0;
}
