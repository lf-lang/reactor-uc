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

DEFINE_REACTION_STRUCT(Receiver, 0, 0)
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
  Environment *env = _self->parent->env;
  gpio_pin_toggle_dt(&led);
  printf("Reaction triggered @ %" PRId64 " (" PRId64 "), " PRId64 ")\n", env->get_elapsed_logical_time(env),
         env->get_logical_time(env), env->get_physical_time(env));
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

DEFINE_FEDERATED_INPUT_CONNECTION(ConnRecv, 1, msg_t, 5, 0, false)

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

typedef struct {
  Reactor super;
  Receiver receiver;
  RecvSenderBundle bundle;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
} MainRecv;

void MainRecv_ctor(MainRecv *self, Environment *env) {
  env->wait_until(env, env->get_physical_time(env) +
                           MSEC(100)); // FIXME: This is a hack until we have proper connection setup
  self->_children[0] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  RecvSenderBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn, self->receiver.inp);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);

  self->_bundles[0] = &self->bundle.super;
}

ENTRY_POINT_FEDERATED(MainRecv, FOREVER, true, true, 1)

int main() {
  setup_led();
  lf_MainRecv_start();
}