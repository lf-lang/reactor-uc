#include "reactor-uc/platform/patmos/patmos.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <reactor-uc/environment.h>
#include "reactor-uc/logging.h"

#include <machine/rtc.h>
#include <machine/exceptions.h>

static PlatformPatmos platform;

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

instant_t PlatformPatmos_get_physical_time(Platform *super) {
  (void)super;
  return USEC(get_cpu_usecs());
}

lf_ret_t PlatformPatmos_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformPatmos *self = (PlatformPatmos *)super;

  instant_t now = super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "PlatformPatmos_wait_until_interruptible: now: %llu sleeping until %llu", now, wakeup_time);

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while (now < wakeup_time);

  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  return LF_OK;
}

lf_ret_t PlatformPatmos_wait_until(Platform *super, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  instant_t now = super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "PlatformPatmos_wait_until: now: %llu sleeping until %llu", now, wakeup_time);

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while (now < wakeup_time);
  return LF_OK;
}

lf_ret_t PlatformPatmos_wait_for(Platform *super, interval_t duration) {
  if (duration <= 0) {
    return LF_OK;
  }

  instant_t now = super->get_physical_time(super);
  instant_t wakeup = now + duration;
  LF_DEBUG(PLATFORM, "PlatformPatmos_wait_for: now: %llu sleeping for %llu", now, duration);

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while (now < wakeup);

  return LF_OK;
}

void PlatformPatmos_leave_critical_section(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
}

void PlatformPatmos_enter_critical_section(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
}

void PlatformPatmos_notify(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
  self->async_event = true;
}

void Platform_ctor(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
  super->get_physical_time = PlatformPatmos_get_physical_time;
  super->wait_until = PlatformPatmos_wait_until;
  super->wait_for = PlatformPatmos_wait_for;
  super->wait_until_interruptible = PlatformPatmos_wait_until_interruptible;
  super->notify = PlatformPatmos_notify;
  self->num_nested_critical_sections = 0;
  LF_DEBUG(PLATFORM, "PlatformPatmos initialized");
}

Platform *Platform_new(void) {
  return (Platform *)&platform;
}

void MutexPatmos_unlock(Mutex *super) {
  MutexPatmos *self = (MutexPatmos *)super;
  PlatformPatmos *platform = (PlatformPatmos *)_lf_environment->platform;
  platform->num_nested_critical_sections--;
  if (platform->num_nested_critical_sections == 0) {
    intr_enable();
  } else if (platform->num_nested_critical_sections < 0) {
    validate(false);
  }
}

void MutexPatmos_lock(Mutex *super) {
  MutexPatmos *self = (MutexPatmos *)super;
  PlatformPatmos *platform = (PlatformPatmos *)_lf_environment->platform;
  if (platform->num_nested_critical_sections == 0) {
    intr_disable();
  }
  platform->num_nested_critical_sections++;
}

void Mutex_ctor(Mutex *super) {
  MutexPatmos *self = (MutexPatmos *)super;
  super->lock = MutexPatmos_lock;
  super->unlock = MutexPatmos_unlock;
}
