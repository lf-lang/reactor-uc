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
  printk("Button pressed!\n");
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
  printk("Button is set up!\n");
}

void setup_led() {
  validate(device_is_ready(led.port));
  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
}

typedef struct {
  char msg[32];
} msg_t;

DEFINE_PHYSICAL_ACTION(Action1, 1, 0, void *, 1, 0, 0)
DEFINE_REACTION(Sender, 0, 0)
DEFINE_OUTPUT_PORT(Out, 1)

typedef struct {
  Reactor super;
  Sender_0 reaction;
  Action1 action;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

REACTION_BODY(Sender, 0, {
  Out *out = &self->out;
  gpio_pin_toggle_dt(&led);
  printf("Timer triggered @ %" PRId64 "\n", env->get_elapsed_logical_time(env));
  msg_t val;
  strcpy(val.msg, "Hello From Sender");
  lf_set(out, val);
})

void Sender_ctor(Sender *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->action;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Sender_0_ctor(&self->reaction, &self->super);
  Action1_ctor(&self->action, &self->super);
  Out_ctor(&self->out, &self->super);
  ACTION_REGISTER_EFFECT(self->action, self->reaction);

  // Register reaction as a source for out
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

DEFINE_FEDERATED_OUTPUT_CONNECTION(ConnSender, msg_t)

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
typedef struct {
  Reactor super;
  Sender sender;
  SenderRecvBundle bundle;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
} MainSender;

void MainSender_ctor(MainSender *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env);
  Reactor_ctor(&self->super, "MainSender", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
  SenderRecvBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_UPSTREAM(self->bundle.conn, self->sender.out);

  self->_bundles[0] = &self->bundle.super;
}

ENTRY_POINT_FEDERATED(MainSender, FOREVER, true, false, 1)

int main() {
  setup_button();
  setup_led();
  lf_MainSender_start();
}