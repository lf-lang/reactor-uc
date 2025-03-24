#include "reactor-uc/physical_clock.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"

lf_ret_t PhysicalClock_set_time(PhysicalClock *self, instant_t time) {
  if (time < 0) {
    return LF_INVALID_VALUE;
  }
  self->env->enter_critical_section(self->env);

  instant_t current_hw_time = self->env->platform->get_physical_time(self->env->platform);
  self->offset = time - current_hw_time;
  // When stepping the clock, also reset the adjustment epoch so that the adjustment is not applied to the new time.
  self->adjustment_epoch_hw = current_hw_time;

  self->env->leave_critical_section(self->env);

  LF_DEBUG(CLOCK_SYNC, "Setting physical clock to " PRINTF_TIME " offset is " PRINTF_TIME, time, self->offset);
  return LF_OK;
}

instant_t PhysicalClock_get_time(PhysicalClock *self) {
  self->env->enter_critical_section(self->env);

  instant_t current_hw_time = self->env->platform->get_physical_time(self->env->platform);
  assert(current_hw_time >= self->adjustment_epoch_hw);
  interval_t time_since_last_adjustment = current_hw_time - self->adjustment_epoch_hw;
  double time_since_last_adjustment_f = (double)time_since_last_adjustment;
  interval_t adjustment = (interval_t)((time_since_last_adjustment_f) * (self->adjustment));
  instant_t ret = current_hw_time + self->offset + adjustment;

  self->env->leave_critical_section(self->env);

  return ret;
}

lf_ret_t PhysicalClock_adjust_time(PhysicalClock *self, interval_t adjustment_ppb) {
  self->env->enter_critical_section(self->env);

  instant_t current_hw_time = self->env->platform->get_physical_time(self->env->platform);
  assert(current_hw_time >= self->adjustment_epoch_hw);
  // Accumulate the old adjustment into the offset.
  interval_t adjustment = (current_hw_time - self->adjustment_epoch_hw) * (self->adjustment);
  self->offset += adjustment;

  // Set a new adjustment and epoch.
  self->adjustment = ((double)adjustment_ppb) / ((double)BILLION);
  self->adjustment_epoch_hw = current_hw_time;

  self->env->leave_critical_section(self->env);

  LF_DEBUG(CLOCK_SYNC, "Adjusting physical clock. Offset: " PRINTF_TIME " adjustment: " PRINTF_TIME, self->offset,
           adjustment_ppb);
  return LF_OK;
}

instant_t PhysicalClock_get_time_no_adjustment(PhysicalClock *self) {
  return self->env->platform->get_physical_time(self->env->platform);
}

instant_t PhysicalClock_to_hw_time(PhysicalClock *self, instant_t time) {
  if (time == FOREVER || time == NEVER) {
    return time;
  }
  self->env->enter_critical_section(self->env);

  // This performs the inverse calculation of `get_time`, where we have
  //  time = hw_time + (hw_time - adjustment_epoch_hw) * adjustment + offset
  // Solved for hw_time we get:
  // hw_time = (time + epoch * adjustment - offset) / (1 + adjustment)
  // To avoid any problems with overflow, underflow etc, we just do the calculation
  // with doubleing point arithmetic.
  double nominator = time + (self->adjustment * self->adjustment_epoch_hw) - self->offset;
  double denominator = 1 + self->adjustment;
  double hw_time = nominator / denominator;

  self->env->leave_critical_section(self->env);
  return (instant_t)hw_time;
}

instant_t PhysicalClock_to_hw_time_no_adjustment(PhysicalClock *self, instant_t time) {
  (void)self;
  return time;
}

void PhysicalClock_ctor(PhysicalClock *self, Environment *env, bool clock_sync_enabled) {
  self->env = env;
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
