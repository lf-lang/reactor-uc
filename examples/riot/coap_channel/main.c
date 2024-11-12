// #include "reactor-uc/reactor-uc.h"
// #include "coap_channel.h"
// #include <inttypes.h>

// typedef struct {
//   Reactor super;
//   Reaction *_reactions[1];
//   Trigger *_triggers[1];
// } MyReactor;

// CoapChannel channel;

// void MyReactor_ctor(MyReactor *self, Environment *env) {
//   Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
//   CoapChannel_ctor(&channel, &env, "127.0.0.1", 4444, "127.0.0.1", 5555);
// }

// /* Application */
// ENTRY_POINT(MyReactor, FOREVER, true)

// int main() {
//   lf_start();
//   return 0;
// }

#include "reactor-uc/reactor-uc.h"
#include "coap_channel.h"

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include "msg.h"

#include "net/gcoap.h"
#include "shell.h"

#define LOCAL_HOST "[::1]"
#define REMOTE_HOST "[::1]"
#define LOCAL_PORT_NUM 8901
#define REMOTE_PORT_NUM 8902

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
  lf_msg_t val;
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

DEFINE_FEDERATED_OUTPUT_CONNECTION(ConnSender, lf_msg_t, 1)

typedef struct {
  FederatedConnectionBundle super;
  CoapChannel channel;
  ConnSender conn;
  FederatedOutputConnection *output[1];
  serialize_hook serialize_hooks[1];
} SenderRecvBundle;

void SenderRecvConn_ctor(SenderRecvBundle *self, Environment *env, Sender *parent) {
  CoapChannel_ctor(&self->channel, env, LOCAL_HOST, LOCAL_PORT_NUM, REMOTE_HOST, REMOTE_PORT_NUM);
  ConnSender_ctor(&self->conn, &parent->super, &self->super);
  self->output[0] = &self->conn.super;
  self->serialize_hooks[0] = serialize_msg_t;

  FederatedConnectionBundle_ctor(&self->super, &parent->super, &self->channel.super, NULL, NULL, 0,
                                 (FederatedOutputConnection **)&self->output, self->serialize_hooks, 1);
}

// Reactor main
typedef struct {
  Reactor super;
  Sender sender;
  SenderRecvBundle bundle;
  CoapChannel channel;
  ConnSender conn;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
  Connection *_conn_sender_out[1];
} MainSender;

void MainSender_ctor(MainSender *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env, self->_conn_sender_out, 1);

  SenderRecvConn_ctor(&self->bundle, env, &self->sender);
  self->_bundles[0] = &self->bundle.super;
  CONN_REGISTER_UPSTREAM(self->bundle.conn, self->sender.out);
  Reactor_ctor(&self->super, "MainSender", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
}

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
  MainSender_ctor(&main_reactor, &env);
  env.net_bundles_size = 1;
  env.net_bundles = (FederatedConnectionBundle **)&main_reactor._bundles;
  // env.assemble(&env);
  // env.leave_critical_section(&env);
  // env.start(&env);
  // lf_exit();
}

int main() {
  lf_start();
  printf("%d\n", env.net_bundles_size);
  NetworkChannel *channel = (NetworkChannel *)&main_reactor.bundle.channel;

  channel->open_connection(channel);
  channel->try_connect(channel);

  puts("All up, running the shell now");
  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
  return 0;
}
