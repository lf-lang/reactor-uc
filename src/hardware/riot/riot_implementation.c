
#include "reactor-uc/hardware_interface.h"

// RIOT ZTIMER Header
#include "ztimer.h"

void hardware_wait_for(instant_t current_time, instant_t wake_up_time) {
  //ztimer_t timeout = { .callback=callback, .arg="Hello ztimer!" };
  //ztimer_set(ZTIMER_SEC, &timeout, 2);
  int64_t time_delta = wake_up_time - current_time;
  int64_t usec = time_delta / 1000;
  ztimer_sleep(ZTIMER_USEC, usec);
}
