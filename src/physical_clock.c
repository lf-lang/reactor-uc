#include "reactor-uc/physical_clock.h"
#include "reactor-uc/logging.h"

lf_ret_t PhysicalClock_set_time(PhysicalClock *self, instant_t time) {
  if (time < 0) {
    return LF_INVALID_VALUE;
  }
  instant_t current_hw_time = self->platform->get_physical_time(self->platform);
  self->offset = time - current_hw_time;
  // When stepping the clock, also reset the adjustment epoch so that the adjustment is not applied to the new time.
  self->adjustment_epoch_hw = current_hw_time;
  LF_DEBUG(CLOCK_SYNC, "Setting physical clock to " PRINTF_TIME " offset is " PRINTF_TIME, time, self->offset);
  return LF_OK;
}

instant_t PhysicalClock_get_time(PhysicalClock *self) {
  instant_t current_hw_time = self->platform->get_physical_time(self->platform);
  assert(current_hw_time >= self->adjustment_epoch_hw);
  interval_t adjustment = (current_hw_time - self->adjustment_epoch_hw) * (self->adjustment);
  return current_hw_time + self->offset + adjustment;
}

lf_ret_t PhysicalClock_adjust_time(PhysicalClock *self, interval_t adjustment_ppb) {
  instant_t current_hw_time = self->platform->get_physical_time(self->platform);
  assert(current_hw_time >= self->adjustment_epoch_hw);
  // Accumulate the old adjustment into the offset.
  interval_t adjustment = (current_hw_time - self->adjustment_epoch_hw) * (self->adjustment);
  self->offset += adjustment;

  // Set a new adjustment and epoch.
  self->adjustment = ((double)adjustment_ppb) / BILLION;
  self->adjustment_epoch_hw = current_hw_time;
  LF_DEBUG(CLOCK_SYNC, "Adjusting physical clock. Offset: " PRINTF_TIME " adjustment: " PRINTF_TIME, self->offset,
           adjustment_ppb);

  return LF_OK;
}

instant_t PhysicalClock_get_time_no_adjustment(PhysicalClock *self) {
  return self->platform->get_physical_time(self->platform);
}

instant_t PhysicalClock_to_hw_time(PhysicalClock *self, instant_t time) {
  // This performs the inverse calculation of `get_time`, where we have
  //  time = hw_time + (hw_time - adjustment_epoch_hw) * ppb / BILLION + offset
  // Solved for hw_time we get:
  // hw_time = (time*BILLION + ppb*epoch - offset*BILLION) / (BILLION + ppb)
  // Here we must be very careful to avoid overflows or underflows since we are doing integer arithmetic.
  double nominator = time + (self->adjustment * self->adjustment_epoch_hw) - self->offset;
  double denominator = 1 + self->adjustment;
  double hw_time = nominator / denominator;
  return (instant_t)hw_time;
}

instant_t PhysicalClock_to_hw_time_no_adjustment(PhysicalClock *self, instant_t time) {
  (void)self;
  return time;
}

void PhysicalClock_ctor(PhysicalClock *self, Platform *platform, bool clock_sync_enabled) {
  self->platform = platform;
  self->offset = 0;
  self->adjustment_epoch_hw = 0;
  self->adjustment = 0.0;
  self->set_time = PhysicalClock_set_time;
  self->adjust_time = PhysicalClock_adjust_time;

  if (clock_sync_enabled) {
    self->get_time = PhysicalClock_get_time;
    self->to_hw_time = PhysicalClock_to_hw_time;
  } else {
    self->get_time = PhysicalClock_get_time_no_adjustment;
    self->to_hw_time = PhysicalClock_to_hw_time_no_adjustment;
  }
}
