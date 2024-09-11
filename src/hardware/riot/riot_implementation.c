
#include "reactor-uc/hardware_interface.h"

#include "clk.h"
#include "board.h"
#include "periph_conf.h"
#include "timex.h"
#include "ztimer.h"

void hardware_wait_for(instant_t current_time, instant_t wake_up_time) {
  //ztimer_t timeout = { .callback=callback, .arg="Hello ztimer!" };
  //ztimer_set(ZTIMER_SEC, &timeout, 2);

  if (IS_USED(MODULE_ZTIMER)) {
    int64_t time_delta = wake_up_time - current_time;
    int64_t usec = time_delta / 1000;
    ztimer_sleep(ZTIMER_USEC, usec);
  } else {
    uint32_t loops = coreclk() / 20;
    for (volatile uint32_t i = 0; i < loops; i++) { }
  }

}
