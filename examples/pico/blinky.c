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

DEFINE_TIMER_STRUCT(Blinky, t, 1);
DEFINE_TIMER_CTOR(Blinky, t, 1);
DEFINE_REACTION_STRUCT(Blinky, r, 1);
DEFINE_REACTION_CTOR(Blinky, r, 0);

struct MyReactor {
  Reactor super;
  TIMER_INSTANCE(Blinky, t);
  REACTION_INSTANCE(Blinky, r);
  bool led_on;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
};

DEFINE_REACTION_BODY(Blinky, r) {
  SCOPE_SELF(Blinky);
  SCOPE_ENV();
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
  pico_set_led(!self->led_on);
  self->led_on = !self->led_on;
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_REACTION(Blinky, r);
  INITIALIZE_TIMER(Blinky, t, MSEC(0), MSEC(500));
  TIMER_REGISTER_EFFECT(t, r);
  self->led_on = false;
}

ENTRY_POINT(Blinky, FOREVER, false);
int main() {
  pico_led_init();
  lf_start();
}
