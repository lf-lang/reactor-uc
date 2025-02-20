#ifndef REACTOR_UC_PHYSICAL_CLOCK_H
#define REACTOR_UC_PHYSICAL_CLOCK_H

#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"
#include "reactor-uc/platform.h"
#include <stdbool.h>

typedef struct PhysicalClock PhysicalClock;

struct PhysicalClock {
  Platform *platform;
  interval_t offset;        // Constant offset applied to each reading of the HW clock
  instant_t adjustment_epoch; // The time at which the frequency adjustment should by applied from.
  interval_t adjustment_ppb;  // The frequency adjustment in parts per billion.
  instant_t (*get_time)(PhysicalClock *self);
  lf_ret_t (*set_time)(PhysicalClock *self, instant_t time);
  lf_ret_t (*adjust_time)(PhysicalClock *self, interval_t adjustment_ppb);
};

void PhysicalClock_ctor(PhysicalClock *self, Platform *platform, bool clock_sync_enabled);

#endif // REACTOR_UC_PHYSICAL_CLOCK_H