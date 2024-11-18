#include "reactor-uc/reactor-uc.h"
#include "coap_udp_ip_channel.h"

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include "msg.h"

#include "net/gcoap.h"
#include "shell.h"

#define REMOTE_HOST "[::1]"

#define MAIN_QUEUE_SIZE (4)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static const shell_command_t shell_commands[] = {{NULL, NULL, NULL}};

typedef struct {
  int size;
  char msg[512];
} lf_msg_t;

size_t serialize_msg_t(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  const lf_msg_t *msg = user_struct;

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
  REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
} Sender;

DEFINE_REACTION_BODY(Sender, r) {
  SCOPE_SELF(Sender);
  SCOPE_ENV();
  SCOPE_PORT(Sender, out);

  printf("Timer triggered @ %" PRId64 "\n", env->get_elapsed_logical_time(env));
  lf_msg_t val;
  strcpy(val.msg, "Hello From Sender");
  val.size = sizeof("Hello From Sender");
  lf_set(out, val);
}

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, Connection **conn_out, size_t conn_out_num) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Sender);
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
  CoapUdpIpChannel channel;
  FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0, 1);
} FEDERATED_CONNECTION_BUNDLE_NAME(Sender, Receiver);

// TODO This macro does not work because I need env here.
// FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver) {
//   FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
//   CoapUdpIpChannel_ctor(&self->channel, env, REMOTE_HOST);

//   FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();

//   INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);
// }

void Sender_Receiver_Bundle_ctor(Sender_Receiver_Bundle *self, Environment *env, Reactor *parent) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  CoapUdpIpChannel_ctor(&self->channel, env, REMOTE_HOST);

  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();

  INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);
}

// Reactor main
typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Sender, sender);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver);
  CoapUdpIpChannel channel;
  FEDERATE_BOOKKEEPING_INSTANCES(0, 0, 1, 1);
  CONTAINED_OUTPUT_CONNECTIONS(sender, out, 1);
} MainSender;

REACTOR_CTOR_SIGNATURE(MainSender) {
  FEDERATE_CTOR_PREAMBLE();
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, self->_conns_sender_out_out, 1);
  // TODO this function has more arguments now because I need env
  // INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver);
  Sender_Receiver_Bundle_ctor(&self->Sender_Receiver_bundle, env, &self->super);
  self->_bundles[_bundle_idx++] = &self->Sender_Receiver_bundle.super;
  BUNDLE_REGISTER_UPSTREAM(Sender, Receiver, sender, out);
  REACTOR_CTOR(MainSender);
}

// ENTRY_POINT_FEDERATED(MainSender, SEC(1), true, false, 1, true)
MainSender main_reactor;
Environment env;
void lf_exit(void) { Environment_free(&env); }
void lf_start() {
  Environment_ctor(&env, (Reactor *)&main_reactor);
  env.scheduler.duration = ((interval_t)(1 * 1000000000LL));
  env.scheduler.keep_alive = 1;
  env.scheduler.leader = 1;
  env.has_async_events = 0;
  env.enter_critical_section(&env);
  MainSender_ctor(&main_reactor, ((void *)0), &env);
  env.net_bundles_size = 1;
  env.net_bundles = (FederatedConnectionBundle **)&main_reactor._bundles;
  // env.assemble(&env);
  // env.leave_critical_section(&env);
  // env.start(&env);
  // lf_exit();
}

// int main() {
//   lf_start();
//   return 0;
// }

uint8_t super_buf[1024];
FederateMessage msg;

#define MESSAGE_CONTENT "Hello World1234"
#define MESSAGE_CONNECTION_ID 42

int main() {
  lf_start();
  NetworkChannel *channel = (NetworkChannel *)&main_reactor.Sender_Receiver_bundle.channel;
  channel->open_connection(channel);

  while (channel->try_connect(channel) != LF_OK) {
    LF_WARN(NET, "Connection not yet established");
  }

  LF_DEBUG(NET, "SUCCESS: All channels connected");

  puts("All up, running the shell now");

  /* create message */
  msg.type = MessageType_TAGGED_MESSAGE;
  msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage *port_message = &msg.message.tagged_message;
  port_message->conn_id = MESSAGE_CONNECTION_ID;
  const char *message = MESSAGE_CONTENT;
  memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
  port_message->payload.size = sizeof(MESSAGE_CONTENT);

  channel->send_blocking(channel, &msg);

  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
  return 0;
}
