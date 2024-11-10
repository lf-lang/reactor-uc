
#include "reactor-uc/reactor-uc.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
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

DEFINE_INPUT_STRUCT(Receiver, in, 1, msg_t, 0)
DEFINE_INPUT_CTOR(Receiver, in, 1, msg_t, 0)

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Receiver, r);
  PORT_INSTANCE(Receiver, in);
  int cnt;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver;

DEFINE_REACTION_BODY(Receiver, r) {
  SCOPE_SELF(Receiver);
  SCOPE_ENV();
  gpio_pin_toggle_dt(&led);
  printf("Reaction triggered @ %" PRId64 " (" PRId64 "), " PRId64 ")\n", env->get_elapsed_logical_time(env),
         env->get_logical_time(env), env->get_physical_time(env));
}

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _reactions_idx = 0;
  size_t _triggers_idx = 0;
  INITIALIZE_REACTION(Receiver, r);
  INITIALIZE_INPUT(Receiver, in);
  
  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(in, r);
  REACTION_REGISTER_EFFECT(r, in);
}
