#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/reactor-uc.h"


#define REMOTE_PROTOCOL_FAMILY AF_INET6

typedef struct {
  int size;
  char msg[512];
} lf_msg_t;

lf_ret_t deserialize_msg_t(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  (void)msg_size;

  lf_msg_t *msg = user_struct;
  memcpy(&msg->size, msg_buf, sizeof(msg->size));
  memcpy(msg->msg, msg_buf + sizeof(msg->size), msg->size);

  return LF_OK;
}

LF_DEFINE_REACTION_STRUCT(Receiver, r, 0)
LF_DEFINE_REACTION_CTOR(Receiver, r, 0, NULL, NULL)
LF_DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, lf_msg_t, 0)
LF_DEFINE_INPUT_CTOR(Receiver, in, 1, 0, lf_msg_t, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Receiver, r);
  LF_PORT_INSTANCE(Receiver, in, 1);
  int cnt;
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
} Receiver;

LF_DEFINE_REACTION_BODY(Receiver, r) {
  LF_SCOPE_SELF(Receiver);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Receiver, in);
  printf("Input triggered @ " PRINTF_TIME " with %s size %d\n", env->get_elapsed_logical_time(env), in->value.msg,
         in->value.size);
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Receiver);
  LF_INITIALIZE_REACTION(Receiver, r, NEVER);
  LF_INITIALIZE_INPUT(Receiver, in, 1, in_external);

  // Register reaction as an effect of in
  LF_PORT_REGISTER_EFFECT(self->in, self->r, 1);
}

LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(Receiver, in, lf_msg_t, 5);
LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(Receiver, in, lf_msg_t, 5, MSEC(100), false, 0);

typedef struct {
  FederatedConnectionBundle super;
  CoapUdpIpChannel channel;
  LF_FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(1, 0)
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Receiver, Sender);

LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  CoapUdpIpChannel_ctor(&self->channel, REMOTE_IP_ADDRESS, REMOTE_PROTOCOL_FAMILY);
  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}

LF_DEFINE_STARTUP_COORDINATOR_STRUCT(Federate, 1, 6);
LF_DEFINE_STARTUP_COORDINATOR_CTOR(Federate, 1, 1, 6);

LF_DEFINE_CLOCK_SYNC_STRUCT(Federate, 1, 2);
LF_DEFINE_CLOCK_SYNC_DEFAULTS_CTOR(Federate, 1, 2, false);

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  LF_FEDERATE_BOOKKEEPING_INSTANCES(1);
  LF_CHILD_INPUT_SOURCES(receiver, in, 1, 1, 0);
  LF_DEFINE_STARTUP_COORDINATOR(Federate);
  LF_DEFINE_CLOCK_SYNC(Federate);
} MainRecv;

LF_REACTOR_CTOR_SIGNATURE(MainRecv) {
  LF_REACTOR_CTOR(MainRecv);
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, _receiver_in_args[i]);
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  LF_INITIALIZE_STARTUP_COORDINATOR(Federate);
  LF_INITIALIZE_CLOCK_SYNC(Federate);
  lf_connect_federated_input(&self->Receiver_Sender_bundle.inputs[0]->super, &self->receiver->in[0].super);
}

LF_ENTRY_POINT_FEDERATED(MainRecv, 32, 32, 32, SEC(1), true, 1, false)

int main() {
  lf_start();
  return 0;
}
