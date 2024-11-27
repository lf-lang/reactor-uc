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

DEFINE_TIMER_STRUCT(Sender, t, 1, 0)
DEFINE_TIMER_CTOR(Sender, t, 1, 0)
DEFINE_REACTION_STRUCT(Sender, r, 1)
DEFINE_REACTION_CTOR(Sender, r, 0)
DEFINE_OUTPUT_STRUCT(Sender, out, 1, msg_t)
DEFINE_OUTPUT_CTOR(Sender, out, 1)  

typedef struct {
  Reactor super;
  TIMER_INSTANCE(Sender, t);
  REACTION_INSTANCE(Sender, r);
  PORT_INSTANCE(Sender, out, 1);
  REACTOR_BOOKKEEPING_INSTANCES(1,2,0);
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

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs *out_external) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Sender);
  INITIALIZE_REACTION(Sender, r);
  INITIALIZE_TIMER(Sender, t, MSEC(0), SEC(1));
  INITIALIZE_OUTPUT(Sender, out, 1, out_external);

  TIMER_REGISTER_EFFECT(self->t, self->r);
  PORT_REGISTER_SOURCE(self->out, self->r, 1);
}

DEFINE_FEDERATED_OUTPUT_CONNECTION(Sender, out, msg_t, 1)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0,1);
} FEDERATED_CONNECTION_BUNDLE_NAME(Sender, Receiver);

FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET, true);

  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  
  INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);
}

// Reactor main
typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Sender, sender, 1);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver);
  TcpIpChannel channel;
  FEDERATE_BOOKKEEPING_INSTANCES(1);
  CHILD_OUTPUT_CONNECTIONS(sender, out, 1,1, 1);
  CHILD_OUTPUT_EFFECTS(sender, out, 1,1, 0);
  CHILD_OUTPUT_OBSERVERS(sender, out, 1,1,0);
} MainSender;

REACTOR_CTOR_SIGNATURE(MainSender) {
  FEDERATE_CTOR_PREAMBLE();
  DEFINE_CHILD_OUTPUT_ARGS(sender, out,1,1);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender,1, _sender_out_args[i]);
  INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver);
  BUNDLE_REGISTER_UPSTREAM(Sender, Receiver, sender, out);
  REACTOR_CTOR(MainSender);
}
ENTRY_POINT_FEDERATED(MainSender, SEC(1), true, false, 1, true)

int main() {
  lf_start();
  return 0;
}
