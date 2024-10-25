#include "pico/stdlib.h"
#include "reactor-uc/reactor-uc.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

// Perform initialisation
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
  // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
  // so we can use normal GPIO functionality to turn the led on and off
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
  // For Pico W devices we need to initialise the driver etc
  return cyw43_arch_init();
#else
#error "No LED pin defined for Pico"
#endif
}

// Turn the led on or off
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
  // Just set the GPIO on or off
  gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
  // Ask the wifi "driver" to set the GPIO on or off
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#else
#error "No LED pin defined for Pico"
#endif
}

typedef struct {
  Timer super;
  Reaction *effects[0];
} MyTimer;

typedef struct {
  Reaction super;
} MyReaction;

struct MyReactor {
  Reactor super;
  MyReaction my_reaction;
  MyTimer timer;
  bool led_on;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
};

void timer_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  Environment *env = self->super.env;
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
  pico_set_led(!self->led_on);
  self->led_on = !self->led_on;
}

void MyReaction_ctor(MyReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, timer_handler, NULL, 0, 0);
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReaction_ctor(&self->my_reaction, &self->super);
  Timer_ctor(&self->timer.super, &self->super, MSEC(0), MSEC(100), self->timer.effects, 1);
  TIMER_REGISTER_EFFECT(self->timer, self->my_reaction);
  self->led_on = false;
}

struct MyReactor my_reactor;
Environment env;
int main() {
  Environment_ctor(&env, (Reactor *)&my_reactor);
  pico_led_init();
  env.scheduler.duration = FOREVER;
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
  return 0;
}
