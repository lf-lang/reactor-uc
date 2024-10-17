#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <zephyr/net/net_ip.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#define PORT_NUM 8901
#define LED0_NODE DT_ALIAS(led0)
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
Action *action_ptr = NULL;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
  action_ptr->schedule(action_ptr, 0, NULL);
}

void setup_button() {
  int ret;

  if (!gpio_is_ready_dt(&button)) {
    printk("Error: button device %s is not ready\n", button.port->name);
    validate(false);
  }

  ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
  if (ret != 0) {
    printk("Error %d: failed to configure %s pin %d\n", ret, button.port->name, button.pin);
    validate(false);
  }

  ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret != 0) {
    printk("Error %d: failed to configure interrupt on %s pin %d\n", ret, button.port->name, button.pin);
    validate(false);
  }

  gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
  gpio_add_callback(button.port, &button_cb_data);
}

void setup_led() {
  validate(device_is_ready(led.port));
  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
}

typedef struct {
  char msg[32];
} msg_t;

// Reactor Sender
typedef struct {
  PhysicalAction super;
  Reaction *effects[1];
} Action1;

void Action1_ctor(Action1 *self, Reactor *parent) {
  PhysicalAction_ctor(&self->super, 0, 0, parent, NULL, 0, self->effects, 1, NULL, 0, 0);
}

typedef struct {
  Reaction super;
} Reaction1;

typedef struct {
  Output super;
  Reaction *sources[1];
  msg_t value;
} Out;

typedef struct {
  Reactor super;
  Reaction1 reaction;
  Action1 action;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

void action_handler(Reaction *_self) {
  Sender *self = (Sender *)_self->parent;
  Environment *env = self->super.env;
  Out *out = &self->out;

  gpio_pin_toggle_dt(&led);
  printf("Timer triggered @ %" PRId64 "\n", env->get_elapsed_logical_time(env));
  msg_t val;
  strcpy(val.msg, "Hello From Sender");
  lf_set(out, val);
}

void Reaction1_ctor(Reaction1 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, action_handler, NULL, 0, 0);
}

void Out_ctor(Out *self, Sender *parent) {
  self->sources[0] = &parent->reaction.super;
  Output_ctor(&self->super, &parent->super, self->sources, 1);
}

void Sender_ctor(Sender *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->action;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction1_ctor(&self->reaction, &self->super);
  Action1_ctor(&self->action, &self->super);
  Out_ctor(&self->out, self);
  ACTION_REGISTER_EFFECT(self->action, self->reaction);

  // Register reaction as a source for out
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

typedef struct {
  FederatedOutputConnection super;
  msg_t buffer[1];
} ConnSender;

void ConnSender_ctor(ConnSender *self, Reactor *parent, FederatedConnectionBundle *bundle) {
  FederatedOutputConnection_ctor(&self->super, parent, bundle, 0, (void *)&self->buffer[0], sizeof(self->buffer[0]));
}

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel chan;
  ConnSender conn;
  FederatedOutputConnection *output[1];
} SenderRecvBundle;

void SenderRecvBundle_ctor(SenderRecvBundle *self, Reactor *parent) {
  TcpIpChannel_ctor(&self->chan, "192.168.1.100", PORT_NUM, AF_INET);
  ConnSender_ctor(&self->conn, parent, &self->super);
  self->output[0] = &self->conn.super;

  NetworkChannel *chan = &self->chan.super;
  int ret = chan->bind(chan);
  validate(ret == LF_OK);
  printf("Sender: Bound\n");

  // accept one connection
  bool new_connection = chan->accept(chan);
  validate(new_connection);
  printf("Sender: Accepted\n");

  FederatedConnectionBundle_ctor(&self->super, parent, &self->chan.super, NULL, 0,
                                 (FederatedOutputConnection **)&self->output, 1);
}

// Reactor main
struct MainSender {
  Reactor super;
  Sender sender;
  SenderRecvBundle bundle;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
};

void MainSender_ctor(struct MainSender *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env);
  Reactor_ctor(&self->super, "MainSender", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
  SenderRecvBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_UPSTREAM(self->bundle.conn, self->sender.out);

  self->_bundles[0] = &self->bundle.super;
}

Environment env_send;
struct MainSender sender;
int main() {
  printf("Sender starting!\n");
  Environment_ctor(&env_send, (Reactor *)&sender);
  MainSender_ctor(&sender, &env_send);
  env_send.net_bundles_size = 1;
  env_send.net_bundles = (FederatedConnectionBundle **)&sender._bundles;
  env_send.assemble(&env_send);
  action_ptr = &sender.sender.action.super.super;

  setup_button();
  setup_led();
  env_send.start(&env_send);
  return 0;
}

void lf_exit(void) { Environment_free(&env_send); }
