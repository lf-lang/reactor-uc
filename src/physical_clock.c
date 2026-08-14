#include "reactor-uc/physical_clock.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"

lf_ret_t PhysicalClock_set_time(PhysicalClock* self, instant_t time) {
  if (time < 0) {
    return LF_INVALID_VALUE;
  }
  MUTEX_LOCK(self->mutex);

  instant_t current_hw_time = self->env->platform->get_physical_time(self->env->platform);
  self->offset = time - current_hw_time;
  // When stepping the clock, also reset the adjustment epoch so that the adjustment is not applied to the new time.
  self->adjustment_epoch_hw = current_hw_time;

  MUTEX_UNLOCK(self->mutex);

  LF_DEBUG(CLOCK_SYNC, "Setting physical clock to " PRINTF_TIME " offset is " PRINTF_TIME, time, self->offset);
  return LF_OK;
}

/**
 * @brief Compute the synchronized time corresponding to @p current_hw_time.
 * The caller must already hold `self->mutex`.
 */
static instant_t time_at_locked(PhysicalClock* self, instant_t current_hw_time) {
  assert(current_hw_time >= self->adjustment_epoch_hw);
  interval_t time_since_last_adjustment = current_hw_time - self->adjustment_epoch_hw;
  double time_since_last_adjustment_f = (double)time_since_last_adjustment;
  interval_t adjustment = (interval_t)(time_since_last_adjustment_f * self->adjustment);
  return lf_time_add(lf_time_add(current_hw_time, self->offset), adjustment);
}

instant_t PhysicalClock_get_time(PhysicalClock* self) {
  MUTEX_LOCK(self->mutex);

  instant_t current_hw_time = self->env->platform->get_physical_time(self->env->platform);
  instant_t ret = time_at_locked(self, current_hw_time);

  MUTEX_UNLOCK(self->mutex);

  return ret;
}

lf_ret_t PhysicalClock_step_time(PhysicalClock* self, interval_t step) {
  MUTEX_LOCK(self->mutex);

  // Read and write under a single acquisition of the mutex, so that the hardware clock
  // cannot advance between the two and the clock is shifted by exactly `step`.
  instant_t current_hw_time = self->env->platform->get_physical_time(self->env->platform);
  instant_t target = lf_time_add(time_at_locked(self, current_hw_time), step);
  if (target < 0) {
    MUTEX_UNLOCK(self->mutex);
    LF_WARN(CLOCK_SYNC, "Refusing to step physical clock by " PRINTF_TIME ", it would go negative", step);
    return LF_INVALID_VALUE;
  }
  self->offset = target - current_hw_time;
  // As in set_time, stepping resets the adjustment epoch so the accrued frequency
  // adjustment is not applied again on top of the new time.
  self->adjustment_epoch_hw = current_hw_time;

  MUTEX_UNLOCK(self->mutex);

  LF_DEBUG(CLOCK_SYNC, "Stepped physical clock by " PRINTF_TIME " to " PRINTF_TIME, step, target);
  return LF_OK;
}

lf_ret_t PhysicalClock_adjust_time(PhysicalClock* self, interval_t adjustment_ppb) {
  MUTEX_LOCK(self->mutex);

  instant_t current_hw_time = self->env->platform->get_physical_time(self->env->platform);
  assert(current_hw_time >= self->adjustment_epoch_hw);
  // Accumulate the old adjustment into the offset.
  interval_t adjustment = (current_hw_time - self->adjustment_epoch_hw) * (self->adjustment);
  self->offset = lf_time_add(self->offset, adjustment);

  // Set a new adjustment and epoch.
  self->adjustment = ((double)adjustment_ppb) / ((double)BILLION);
  self->adjustment_epoch_hw = current_hw_time;

  MUTEX_UNLOCK(self->mutex);

  LF_DEBUG(CLOCK_SYNC, "Adjusting physical clock. Offset: " PRINTF_TIME " adjustment: " PRINTF_TIME, self->offset,
           adjustment_ppb);
  return LF_OK;
}

instant_t PhysicalClock_get_time_no_adjustment(PhysicalClock* self) {
  return self->env->platform->get_physical_time(self->env->platform);
}

instant_t PhysicalClock_to_hw_time(PhysicalClock* self, instant_t time) {
  if (time == FOREVER || time == NEVER) {
    return time;
  }
  MUTEX_LOCK(self->mutex);

  // This performs the inverse calculation of `get_time`, where we have
  //  time = hw_time + (hw_time - adjustment_epoch_hw) * adjustment + offset
  // Solved for hw_time, and rebased on the epoch so that the division operates on an
  // elapsed interval rather than an absolute instant:
  //  hw_time = adjustment_epoch_hw + (time - (adjustment_epoch_hw + offset)) / (1 + adjustment)
  //
  // Only the elapsed interval is handed to floating point. Evaluating the whole
  // expression in double would silently lose nanoseconds: a double has a 53-bit
  // mantissa, so at the magnitude of a POSIX epoch clock (~1.75e18 ns) one ulp is
  // already 256 ns, which would make this function non-injective.
  instant_t hw_time;
  if (self->adjustment == 0.0) {
    // No frequency adjustment, so the transform is a pure integer shift and needs no
    // floating point at all. 
    hw_time = lf_time_add(time, -self->offset);
  } else {
    instant_t epoch = self->adjustment_epoch_hw;
    interval_t elapsed_sync = lf_time_add(time, -lf_time_add(epoch, self->offset));
    interval_t elapsed_hw = (interval_t)(((double)elapsed_sync) / (1.0 + self->adjustment));
    hw_time = lf_time_add(epoch, elapsed_hw);
  }

  MUTEX_UNLOCK(self->mutex);
  return hw_time;
}

instant_t PhysicalClock_to_hw_time_no_adjustment(PhysicalClock* self, instant_t time) {
  (void)self;
  return time;
}

void PhysicalClock_ctor(PhysicalClock* self, Environment* env, bool clock_sync_enabled) {
  self->env = env;
  self->offset = 0;
  self->adjustment_epoch_hw = 0;
  self->adjustment = 0.0;
  self->set_time = PhysicalClock_set_time;
  self->step_time = PhysicalClock_step_time;
  self->adjust_time = PhysicalClock_adjust_time;

  Mutex_ctor(&self->mutex.super);

  if (clock_sync_enabled) {
    self->get_time = PhysicalClock_get_time;
    self->to_hw_time = PhysicalClock_to_hw_time;
  } else {
    self->get_time = PhysicalClock_get_time_no_adjustment;
    self->to_hw_time = PhysicalClock_to_hw_time_no_adjustment;
  }
}
