#include "reactor-uc/reactor-uc.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include "../../../common/timer_source.h" 
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void setup_led() {
  validate(device_is_ready(led.port));
  gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
}


LF_DEFINE_REACTION_BODY(TimerSource, r) {
  LF_SCOPE_SELF(TimerSource);
  LF_SCOPE_ENV();
  printf("TimerSource World @ %lld\n", env->get_elapsed_logical_time(env));
  gpio_pin_toggle_dt(&led);
}

int main() {
  lf_start();
}