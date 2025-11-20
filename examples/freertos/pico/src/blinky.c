#include "pico/stdlib.h"
#include "reactor-uc/reactor-uc.h"
#include "../../common/timer_source.h"
#include "blinky.h"

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
void pico_toggle_led(Environment *env) {
 static bool led_on = false;
 led_on = !led_on;
 printf("Turning LED %s, @ lt=%lld, pt=%lld\n", led_on ? "ON" : "OFF", env->get_elapsed_logical_time(env), env->get_physical_time(env));
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

LF_DEFINE_REACTION_BODY(TimerSource, r) {
  LF_SCOPE_SELF(TimerSource);
  LF_SCOPE_ENV();
  pico_toggle_led(env);
}

LF_DEFINE_REACTION_BODY(TimerSource, s) {
  LF_SCOPE_SELF(TimerSource);
  LF_SCOPE_ENV();
  LF_SCOPE_STARTUP(TimerSource);
  // Configure the LED on startup
  printf("Initializing LED\n");
  pico_led_init();
}

void main_pico() {
  lf_start();
}

int main() {
  stdio_init_all();
  xTaskCreate(main_pico, "main_pico", configMINIMAL_STACK_SIZE*2, NULL, tskIDLE_PRIORITY + 2, NULL);
  vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the Idle and/or
	timer tasks to be created.  See the memory management section on the
	FreeRTOS web site for more details on the FreeRTOS heap
	http://www.freertos.org/a00111.html. */
	for( ;; );

  return 0;
}
