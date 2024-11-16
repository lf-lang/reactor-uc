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
DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, msg_t, 0);
DEFINE_INPUT_CTOR(Receiver, in, 1, 0, msg_t, 0);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Receiver, r);
  PORT_INSTANCE(Receiver, in);
  int cnt;
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} Receiver;

DEFINE_REACTION_BODY(Receiver, r) {
  SCOPE_SELF(Receiver);
  SCOPE_ENV();
  SCOPE_PORT(Receiver, in);
  printf("Input triggered @ %" PRId64 " with %s size %d\n", env->get_elapsed_logical_time(env), in->value.msg,
         in->value.size);
}

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs in_external) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Receiver);
  INITIALIZE_REACTION(Receiver, r);
  INITIALIZE_INPUT(Receiver, in, in_external);

  // Register reaction as an effect of in
  PORT_REGISTER_EFFECT(in, r);
}

DEFINE_FEDERATED_INPUT_CONNECTION(Receiver, in, msg_t, 5, MSEC(100), false);

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(1,0)
} FEDERATED_CONNECTION_BUNDLE_NAME(Receiver, Sender);

FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, "127.0.0.1", PORT_NUM, AF_INET, false);
  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}

typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Receiver, receiver);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  FEDERATE_BOOKKEEPING_INSTANCES(0,0,1,1);
  CHILD_INPUT_SOURCES(receiver, in, 0);
} MainRecv;

REACTOR_CTOR_SIGNATURE(MainRecv) {
  FEDERATE_CTOR_PREAMBLE();
  REACTOR_CTOR(MainRecv);
  DEFINE_CHILD_INPUT_ARGS(receiver, in);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, _receiver_in_args);
  INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  BUNDLE_REGISTER_DOWNSTREAM(Receiver, Sender, receiver, in);
}

ENTRY_POINT_FEDERATED(MainRecv, SEC(1), true, true, 1, false)

int main() {
  lf_start();
  return 0;
}
