#ifndef REACTOR_UC_PHYSICAL_CLOCK_H
#define REACTOR_UC_PHYSICAL_CLOCK_H

#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"
#include <stdbool.h>

typedef struct PhysicalClock PhysicalClock;
typedef struct Environment Environment;

struct PhysicalClock {
  Environment *env;
  interval_t offset;             // Constant offset applied to each reading of the HW clock
  instant_t adjustment_epoch_hw; // The time at which the frequency adjustment should by applied from.
  double adjustment;             // The frequency adjustment factor.
  /**
   * @brief Get the current, synchronized, physical time.
   *
   * If this function is called from a context outside the runtime, it must be in a critical section!
   * When called from the runtime context, it does not need to be in a critical section.
   */
  instant_t (*get_time)(PhysicalClock *self);

  /**
   * @brief Step the clock backwards or forwards.
   *
   * Should only ever be called from the runtime context and within a critical section.
   */
  lf_ret_t (*set_time)(PhysicalClock *self, instant_t time);

  /**
   * @brief Change the adjustment applied to the underlying wallclock.
   *
   * Should only ever be called from the runtime context and within a critical section.
   */
  lf_ret_t (*adjust_time)(PhysicalClock *self, interval_t adjustment_ppb);

  /**
   * @brief Translate a physical, synchronized, time instant to the corresponding wall-clock
   * time instant. This is needed in order to correctly tell the platform how long to
   * sleep.
   */
  instant_t (*to_hw_time)(PhysicalClock *self, instant_t time);
};

void PhysicalClock_ctor(PhysicalClock *self, Environment *env, bool clock_sync_enabled);

#endif // REACTOR_UC_PHYSICAL_CLOCK_H