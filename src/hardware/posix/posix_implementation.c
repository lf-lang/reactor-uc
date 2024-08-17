#include "reactor-uc/hardware_interface.h"

#include <time.h>

void hardware_wait_for(instant_t current_time, instant_t wake_up_time) {
  int64_t time_delta_nanoseconds = wake_up_time - current_time;

  struct timespec ts;
  int res;

  if (time_delta_nanoseconds < 0) {
    return;
  }

  ts.tv_sec = time_delta_nanoseconds / 1000000000;
  ts.tv_nsec = (time_delta_nanoseconds % 1000000000);

  do {
    res = nanosleep(&ts, &ts);
  } while (res);

}
