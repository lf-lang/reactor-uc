#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/serialization.h"
#include <zephyr/net/net_ip.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#define PORT_CONN_1 8901
#define PORT_CONN_2 8902
#define LED0_NODE DT_ALIAS(led0)
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

typedef struct {
  char msg[32];
} msg_t;

DEFINE_ACTION_STRUCT(Sender, act, PHYSICAL_ACTION, 1, 0, 0, 10, bool);
DEFINE_ACTION_CTOR(Sender, act, PHYSICAL_ACTION, 1, 0, 0, 10, bool);
DEFINE_REACTION_STRUCT(Sender, r, 1);
DEFINE_REACTION_CTOR(Sender, r, 0);

DEFINE_OUTPUT_STRUCT(Sender, out, 1, msg_t)
DEFINE_OUTPUT_CTOR(Sender, out, 1)

Sender_act *action_ptr = NULL;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
  printk("Button pressed!\n");
  lf_schedule(action_ptr, 0, true);
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
  Reactor super;
  REACTION_INSTANCE(Sender, r);
  ACTION_INSTANCE(Sender, act);
  PORT_INSTANCE(Sender, out);
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} Sender;

DEFINE_REACTION_BODY(Sender, r) {
  SCOPE_SELF(Sender);
  SCOPE_ENV();
  SCOPE_PORT(Sender, out);
  gpio_pin_toggle_dt(&led);
  printf("Reaction triggered @ %" PRId64 " (%" PRId64 "), %" PRId64 ")\n", env->get_elapsed_logical_time(env),
         env->get_logical_time(env), env->get_physical_time(env));
  msg_t val;
  strcpy(val.msg, "Hello From Sender");
  lf_set(out, val);
}

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs out_external) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Sender);
  INITIALIZE_REACTION(Sender, r);
  INITIALIZE_ACTION(Sender, act, MSEC(0));
  INITIALIZE_OUTPUT(Sender, out, out_external);

  ACTION_REGISTER_EFFECT(act, r);
  REACTION_REGISTER_EFFECT(r, out);
  PORT_REGISTER_SOURCE(out, r);
}

DEFINE_FEDERATED_OUTPUT_CONNECTION(Sender, out, msg_t, 1)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0,1);
} FEDERATED_CONNECTION_BUNDLE_NAME(Sender, Receiver1);

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0,1);
} FEDERATED_CONNECTION_BUNDLE_NAME(Sender, Receiver2);

FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver1) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, "192.168.1.100", PORT_CONN_1, AF_INET, true);

  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  
  INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_payload_default);
}

FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver2) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, "192.168.1.100", PORT_CONN_2, AF_INET, true);

  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  
  INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_payload_default);
}

// Reactor main
typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Sender, sender);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver1);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver2);
  CHILD_OUTPUT_CONNECTIONS(sender, out, 2);
  CHILD_OUTPUT_EFFECTS(sender, out, 0);
  CHILD_OUTPUT_OBSERVERS(sender, out, 0);
  FEDERATE_BOOKKEEPING_INSTANCES(0,0,1,2);
} MainSender;

REACTOR_CTOR_SIGNATURE(MainSender) {
  FEDERATE_CTOR_PREAMBLE();
  REACTOR_CTOR(MainSender);

  DEFINE_CHILD_OUTPUT_ARGS(sender, out);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, _sender_out_args);
  INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver1);
  INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver2);

  BUNDLE_REGISTER_UPSTREAM(Sender, Receiver1, sender, out);
  BUNDLE_REGISTER_UPSTREAM(Sender, Receiver2, sender, out);
}

ENTRY_POINT_FEDERATED(MainSender, FOREVER, true, true, 2, true)

int main() {
  setup_button();
  setup_led();
  action_ptr = &main_reactor.sender.act;
  lf_start();
}