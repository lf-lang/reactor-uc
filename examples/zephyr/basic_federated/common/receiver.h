
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

LF_DEFINE_REACTION_STRUCT(Receiver, r, 0);
LF_DEFINE_REACTION_CTOR(Receiver, r, 0)

LF_DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, msg_t, 0)
LF_DEFINE_INPUT_CTOR(Receiver, in, 1, 0, msg_t, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Receiver, r);
  LF_PORT_INSTANCE(Receiver, in, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
  int cnt;
} Receiver;

LF_DEFINE_REACTION_BODY(Receiver, r) {
  LF_SCOPE_SELF(Receiver);
  LF_SCOPE_ENV();
  gpio_pin_toggle_dt(&led);
  printf("Reaction triggered @ %" PRId64 " (%" PRId64 "), %" PRId64 ")\n", env->get_elapsed_logical_time(env),
         env->get_logical_time(env), env->get_physical_time(env));
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Receiver);
  LF_INITIALIZE_REACTION(Receiver, r);
  LF_INITIALIZE_INPUT(Receiver, in, 1, in_external);
  
  // Register reaction as an effect of in
  LF_PORT_REGISTER_EFFECT(self->in, self->r, 1);
}


LF_DEFINE_FEDERATED_INPUT_CONNECTION(Receiver, in, msg_t, 5, MSEC(100), false);

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel channel;
  LF_FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(1, 0)
} LF_FEDERATED_CONNECTION_BUNDLE_NAME(Receiver, Sender);


LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  TcpIpChannel_ctor(&self->channel, parent->env, IP_ADDR, PORT_NUM, AF_INET, false);
  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_payload_default);
}

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  LF_FEDERATE_BOOKKEEPING_INSTANCES(1);
  LF_CHILD_INPUT_SOURCES(receiver, in, 1, 1, 0);
} MainRecv;

LF_REACTOR_CTOR_SIGNATURE(MainRecv) {
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(MainRecv);
  LF_DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, _receiver_in_args[i]);
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  LF_BUNDLE_REGISTER_DOWNSTREAM(Receiver, Sender, receiver, in);
}
LF_ENTRY_POINT_FEDERATED(MainRecv, FOREVER, true, true, 1, false)
