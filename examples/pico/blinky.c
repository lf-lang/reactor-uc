#include "pico/stdlib.h"
#include "reactor-uc/reactor-uc.h"
#include "../common/timer_source.h"

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

// Toggle the led
void pico_toggle_led() {
 static bool led_on = false;
 led_on = !led_on;
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

DEFINE_REACTION_BODY(TimerSource, r) {
  SCOPE_SELF(TimerSource);
  SCOPE_ENV();
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
  pico_toggle_led();
}

int main() {
  pico_led_init();
  lf_start();
}
