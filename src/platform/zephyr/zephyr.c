#include "reactor-uc/platform/zephyr/zephyr.h"
#include "reactor-uc/logging.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "zephyr/sys/time_units.h"
#include <zephyr/fatal_types.h>

static PlatformZephyr platform;

void Platform_vprintf(const char *fmt, va_list args) { vprintk(fmt, args); }

// Catch kernel panics from Zephyr
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf) {
  (void)esf;
  LF_ERR(PLATFORM, "Zephyr kernel panic reason=%d", reason);
  throw("Zephyr kernel panic");
}

lf_ret_t PlatformZephyr_initialize(Platform *super) {
  PlatformZephyr *self = (PlatformZephyr *)super;
  int ret = k_sem_init(&self->sem, 0, 1);
  if (ret == 0) {
    return LF_OK;
  } else {
    LF_ERR(PLATFORM, "Failed to initialize semaphore");
    return LF_ERR;
  }
}

// TODO: k_uptime_get has msec-level accuracy. It can be worth investigating other kernel APIs.
instant_t PlatformZephyr_get_physical_time(Platform *super) {
  (void)super;
  return k_uptime_get() * MSEC(1);
}

lf_ret_t PlatformZephyr_wait_for(Platform *super, interval_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  int32_t sleep_duration_usec;
  while (duration > 0) {
    if (duration > USEC(INT32_MAX)) {
      sleep_duration_usec = INT32_MAX;
    } else {
      sleep_duration_usec = duration / 1000;
    }
    duration -= USEC(sleep_duration_usec);
    LF_DEBUG(PLATFORM, "Waiting duration %d usec", sleep_duration_usec);
    k_usleep(sleep_duration_usec);
  }

  return LF_OK;
}

lf_ret_t PlatformZephyr_wait_until(Platform *super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  return PlatformZephyr_wait_for(super, sleep_duration);
}

lf_ret_t PlatformZephyr_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformZephyr *platform = (PlatformZephyr *)self;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "Wait until interruptible for " PRINTF_TIME, wakeup_time);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  k_sem_reset(&self->sem);

  super->leave_critical_section(super);
  int ret = k_sem_take(&self->sem, K_NSEC(sleep_duration));
  super->enter_critical_section(super);

  if (ret == 0) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    return LF_SLEEP_INTERRUPTED;
  } else if (ret == -EAGAIN ||
             ret == -EBUSY) { // EAGAIN means that we timed out. EBUSY means we passed in a wait of zero.
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  } else {
    LF_ERR(PLATFORM, "Wait until failed with %d", ret);
    return LF_ERR;
  }
}

void PlatformZephyr_leave_critical_section(Platform *super) {
  PlatformZephyr *self = (PlatformZephyr *)super;
  LF_DEBUG(PLATFORM, "Leave critical section");
  self->num_nested_critical_sections--;
  if (self->num_nested_critical_sections == 0) {
    irq_unlock(self->irq_mask);
  } else if (self->num_nested_critical_sections < 0) {
    validate(false);
  }
}

void PlatformZephyr_enter_critical_section(Platform *super) {
  PlatformZephyr *self = (PlatformZephyr *)super;
  LF_DEBUG(PLATFORM, "Enter critical section");
  if (self->num_nested_critical_sections == 0) {
    self->irq_mask = irq_lock();
  }
  self->num_nested_critical_sections++;
}

void PlatformZephyr_new_async_event(Platform *super) {
  PlatformZephyr *self = (PlatformZephyr *)super;
  LF_DEBUG(PLATFORM, "New async event");
  k_sem_give(&self->sem);
}

void Platform_ctor(Platform *super) {
  PlatformZephyr *self = (PlatformZephyr *)super;
  super->enter_critical_section = PlatformZephyr_enter_critical_section;
  super->leave_critical_section = PlatformZephyr_leave_critical_section;
  super->get_physical_time = PlatformZephyr_get_physical_time;
  super->wait_until = PlatformZephyr_wait_until;
  super->wait_for = PlatformZephyr_wait_for;
  super->initialize = PlatformZephyr_initialize;
  super->wait_until_interruptible_locked = PlatformZephyr_wait_until_interruptible;
  super->new_async_event = PlatformZephyr_new_async_event;
  self->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
