
#include "reactor-uc/reactor-uc.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "reactor-uc/logging.h"
#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/serialization.h"
#include <zephyr/net/net_ip.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#ifndef PORT_NUM
#error "PORT_NUM must be defined"
#endif

#ifndef IP_ADDR
#error "IP_ADDR must be defined"
#endif

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void setup_led() {
  validate(device_is_ready(led.port));
  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
}

typedef struct {
  char msg[32];
} msg_t;

DEFINE_REACTION_STRUCT(Receiver, r,  0);
DEFINE_REACTION_CTOR(Receiver, r, 0)

DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, msg_t, 0)
DEFINE_INPUT_CTOR(Receiver, in, 1, 0, msg_t, 0)

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Receiver, r);
  PORT_INSTANCE(Receiver, in, 1);
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
  int cnt;
} Receiver;

DEFINE_REACTION_BODY(Receiver, r) {
  SCOPE_SELF(Receiver);
  SCOPE_ENV();
  gpio_pin_toggle_dt(&led);
  printf("Reaction triggered @ %" PRId64 " (%" PRId64 "), %" PRId64 ")\n", env->get_elapsed_logical_time(env),
         env->get_logical_time(env), env->get_physical_time(env));
}

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Receiver);
  INITIALIZE_REACTION(Receiver, r);
  INITIALIZE_INPUT(Receiver, in, 1, in_external);
  
  // Register reaction as an effect of in
  PORT_REGISTER_EFFECT(self->in, self->r, 1);
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
  TcpIpChannel_ctor(&self->channel, IP_ADDR, PORT_NUM, AF_INET, false);
  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_payload_default);
}

typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  FEDERATE_BOOKKEEPING_INSTANCES(1);
  CHILD_INPUT_SOURCES(receiver, in, 1,1, 0);
} MainRecv;

REACTOR_CTOR_SIGNATURE(MainRecv) {
  FEDERATE_CTOR_PREAMBLE();
  REACTOR_CTOR(MainRecv);
  DEFINE_CHILD_INPUT_ARGS(receiver, in,1,1);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver,1, _receiver_in_args[i]);
  INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  BUNDLE_REGISTER_DOWNSTREAM(Receiver, Sender, receiver, in);
}
ENTRY_POINT_FEDERATED(MainRecv, FOREVER, true, true, 1, false)
