#ifndef REACTOR_UC_PHYSICAL_CLOCK_H
#define REACTOR_UC_PHYSICAL_CLOCK_H

#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"

typedef struct PhysicalClock PhysicalClock;

struct PhysicalClock {
  int drift_correction_ppb;  // Drift correction applied to the HW clock, in parts per billion
  interval_t offset;        // Constant offset applied to each reading of the HW clock
  instant_t drift_correction_hw_epoch; // The HW time which the drift correction should be applied from. 

  instant_t (*from_hardware_time)(PhysicalClock *self, instant_t hw_time);
  instant_t (*to_hardware_time)(PhysicalClock *self, instant_t time);
  lf_ret_t (*set_clock)(PhysicalClock *self, instant_t time, instant_t current_hw_time);
  lf_ret_t (*step_clock)(PhysicalClock *self, interval_t step);
  lf_ret_t (*adjust_frequency)(PhysicalClock *self, int ppb, instant_t current_hw_time);
};

#endif // REACTOR_UC_PHYSICAL_CLOCK_H