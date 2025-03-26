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

int serialize_msg_t(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  const msg_t *msg = user_struct;

  memcpy(msg_buf, &msg->size, sizeof(msg->size));
  memcpy(msg_buf + sizeof(msg->size), msg->msg, msg->size);

  return sizeof(msg->size) + msg->size;
}

LF_DEFINE_TIMER_STRUCT(Sender, t, 1, 0)
LF_DEFINE_TIMER_CTOR(Sender, t, 1, 0)
LF_DEFINE_REACTION_STRUCT(Sender, r, 1)
LF_DEFINE_REACTION_CTOR(Sender, r, 0, NULL, NEVER, NULL)
LF_DEFINE_OUTPUT_STRUCT(Sender, out, 1, msg_t)
LF_DEFINE_OUTPUT_CTOR(Sender, out, 1)

typedef struct {
  Reactor super;
  LF_TIMER_INSTANCE(Sender, t);
  LF_REACTION_INSTANCE(Sender, r);
  LF_PORT_INSTANCE(Sender, out, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 2, 0);
} Sender;

LF_DEFINE_REACTION_BODY(Sender, r) {
  LF_SCOPE_SELF(Sender);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Sender, out);

  printf("Timer triggered @ " PRINTF_TIME "\n", env->get_elapsed_logical_time(env));
  msg_t val;
  strcpy(val.msg, "Hello From Sender");
  val.size = sizeof("Hello From Sender");
  lf_set(out, val);
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs *out_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Sender);
  LF_INITIALIZE_REACTION(Sender, r);
  LF_INITIALIZE_TIMER(Sender, t, MSEC(0), SEC(1));
  LF_INITIALIZE_OUTPUT(Sender, out, 1, out_external);

  LF_TIMER_REGISTER_EFFECT(self->t, self->r);
  LF_PORT_REGISTER_SOURCE(self->out, self->r, 1);
}

LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(Sender, out, msg_t)
LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(Sender, out, msg_t, 0)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0, 1);
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Sender, Receiver);

LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET, true);

  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();

  LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);
}

LF_DEFINE_STARTUP_COORDINATOR_STRUCT(Federate, 1, 6);
LF_DEFINE_STARTUP_COORDINATOR_CTOR(Federate, 1, 1, 6);

LF_DEFINE_CLOCK_SYNC_STRUCT(Federate, 1, 2);
LF_DEFINE_CLOCK_SYNC_DEFAULTS_CTOR(Federate, 1, 2, true);

// Reactor main
typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Sender, sender, 1);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver);
  LF_FEDERATE_BOOKKEEPING_INSTANCES(1);
  LF_CHILD_OUTPUT_CONNECTIONS(sender, out, 1, 1, 1);
  LF_CHILD_OUTPUT_EFFECTS(sender, out, 1, 1, 0);
  LF_CHILD_OUTPUT_OBSERVERS(sender, out, 1, 1, 0);
  LF_DEFINE_STARTUP_COORDINATOR(Federate);
  LF_DEFINE_CLOCK_SYNC(Federate);
} MainSender;

LF_REACTOR_CTOR_SIGNATURE(MainSender) {
  LF_REACTOR_CTOR(MainSender);
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_DEFINE_CHILD_OUTPUT_ARGS(sender, out, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, 1, _sender_out_args[i]);
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver);
  lf_connect_federated_output(&self->Sender_Receiver_bundle.outputs[0]->super, &self->sender->out[0].super);
  LF_INITIALIZE_STARTUP_COORDINATOR(Federate);
  LF_INITIALIZE_CLOCK_SYNC(Federate);
}

LF_ENTRY_POINT_FEDERATED(MainSender,32,32,32, SEC(1), true, 1, true)

int main() {
  lf_start();
  return 0;
}
