#include "reactor-uc/physical_clock.h"
#include "reactor-uc/logging.h"

// FIXME: Think about how to handle the case where the clock is stepped back.
// FIXME: How to handle the case where the clock is stepped forward.
// FIXME: How to handle concurrency?

lf_ret_t PhysicalClock_set_time(PhysicalClock *self, instant_t time) {
  self->offset = time - self->platform->get_physical_time(self->platform);
  // When stepping the clock, also reset the adjustment epoch so that the adjustment is not applied to the new time.
  self->adjustment_epoch = time;
  return LF_OK;
}

instant_t PhysicalClock_get_time(PhysicalClock *self) {
  instant_t time = self->platform->get_physical_time(self->platform);
  assert(time >= self->adjustment_epoch);
  interval_t adjustment = (time - self->adjustment_epoch) * self->adjustment_ppb / BILLION;
  self->offset += adjustment;
  self->adjustment_epoch = time;
  return time + self->offset;
}

lf_ret_t PhysicalClock_adjust_time(PhysicalClock *self, interval_t adjustment_ppb) {
  instant_t new_epoch = self->platform->get_physical_time(self->platform);
  assert(new_epoch >= self->adjustment_epoch);
  self->offset += (new_epoch - self->adjustment_epoch) * self->adjustment_ppb / BILLION;
  self->adjustment_ppb = adjustment_ppb;
  self->adjustment_epoch = new_epoch;
  return LF_OK;
}

instant_t PhysicalClock_get_time_no_adjustment(PhysicalClock *self) {
  return self->platform->get_physical_time(self->platform);
}

void PhysicalClock_ctor(PhysicalClock *self, Platform *platform, bool clock_sync_enabled) {
  self->platform = platform;
  self->offset = 0;
  self->adjustment_epoch = 0;
  self->adjustment_ppb = 0;
  self->set_time = PhysicalClock_set_time;
  self->adjust_time = PhysicalClock_adjust_time;

  if (clock_sync_enabled) {
    self->get_time = PhysicalClock_get_time;
  } else {
    self->get_time = PhysicalClock_get_time_no_adjustment;
  }
}
