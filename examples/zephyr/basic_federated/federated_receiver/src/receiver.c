#include "reactor-uc/logging.h"
#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <zephyr/net/net_ip.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#define PORT_NUM 8901
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void setup_led() {
  validate(device_is_ready(led.port));
  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
}

typedef struct {
  char msg[32];
} msg_t;

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

  gpio_pin_toggle_dt(&led);
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
  FederatedInputConnection super;
  msg_t buffer[5];
  Input *downstreams[1];
} ConnRecv;

void ConnRecv_ctor(ConnRecv *self, Reactor *parent) {
  FederatedInputConnection_ctor(&self->super, parent, 0, false, (Port **)&self->downstreams, 1, &self->buffer[0],
                                sizeof(self->buffer[0]), 5);
}

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel chan;
  ConnRecv conn;
  FederatedInputConnection *inputs[1];
} RecvSenderBundle;

void RecvSenderBundle_ctor(RecvSenderBundle *self, Reactor *parent) {
  ConnRecv_ctor(&self->conn, parent);
  TcpIpChannel_ctor(&self->chan, "192.168.1.100", PORT_NUM, AF_INET);
  self->inputs[0] = &self->conn.super;

  NetworkChannel *chan = &self->chan.super;

  lf_ret_t ret;
  LF_DEBUG(ENV, "Recv: Connecting");
  do {
    ret = chan->connect(chan);
  } while (ret != LF_OK);
  LF_DEBUG(ENV, "Recv: Connected");

  FederatedConnectionBundle_ctor(&self->super, parent, &self->chan.super, (FederatedInputConnection **)&self->inputs, 1,
                                 NULL, 0);
}

struct MainRecv {
  Reactor super;
  Receiver receiver;
  RecvSenderBundle bundle;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
};

void MainRecv_ctor(struct MainRecv *self, Environment *env) {
  self->_children[0] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  RecvSenderBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn, self->receiver.inp);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);

  self->_bundles[0] = &self->bundle.super;
}

Environment env_recv;
struct MainRecv receiver;
int main(void) {
  printf("Receiver starting!\n");
  Environment_ctor(&env_recv, (Reactor *)&receiver);
  env_recv.platform->enter_critical_section(env_recv.platform);
  MainRecv_ctor(&receiver, &env_recv);
  env_recv.scheduler.keep_alive = true;
  env_recv.has_async_events = true;
  env_recv.net_bundles_size = 1;
  env_recv.net_bundles = (FederatedConnectionBundle **)&receiver._bundles;
  env_recv.assemble(&env_recv);
  env_recv.platform->leave_critical_section(env_recv.platform);
  setup_led();
  env_recv.start(&env_recv);
  return 0;
}

void lf_exit(void) { Environment_free(&env_recv); }
