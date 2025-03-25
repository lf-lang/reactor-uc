#include "reactor-uc/platform/riot/riot.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "reactor-uc/logging.h"

#include "irq.h"
#include "mutex.h"
#include "ztimer64.h"

static PlatformRiot platform;

#define USEC_TO_NSEC(usec) (usec * USEC(1))
#define NSEC_TO_USEC(nsec) (nsec / USEC(1))

void Platform_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }

lf_ret_t PlatformRiot_initialize(Platform *super) {
  mutex_init(&((PlatformRiot *)self)->lock);
  mutex_lock(&((PlatformRiot *)self)->lock);
  return LF_OK;
}

instant_t PlatformRiot_get_physical_time(Platform *super) {
  (void)self;

  return USEC_TO_NSEC((ztimer64_now(ZTIMER64_USEC)));
}

lf_ret_t PlatformRiot_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "Wait until interruptible for " PRINTF_TIME " ns", sleep_duration);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  super->leave_critical_section(super);
  int ret = ztimer64_mutex_lock_until(ZTIMER64_USEC, &((PlatformRiot *)self)->lock, NSEC_TO_USEC(wakeup_time));
  super->enter_critical_section(super);

  if (ret == 0) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    // the mutex was unlocked from IRQ (no timout occurred)
    return LF_SLEEP_INTERRUPTED;
  } else {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  }
}

lf_ret_t PlatformRiot_wait_until(Platform *super, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  ztimer64_sleep_until(ZTIMER64_USEC, NSEC_TO_USEC(wakeup_time));
  return LF_OK;
}

lf_ret_t PlatformRiot_wait_for(Platform *super, interval_t duration) {
  (void)self;
  if (duration <= 0)
    return LF_OK;
  ztimer64_sleep(ZTIMER64_USEC, NSEC_TO_USEC(duration));
  return LF_OK;
}

void PlatformRiot_leave_critical_section(Platform *super) {
  PlatformRiot *platform = (PlatformRiot *)self;
  self->num_nested_critical_sections--;
  if (self->num_nested_critical_sections == 0) {
    irq_restore(self->irq_mask);
  } else if (self->num_nested_critical_sections < 0) {
    validate(false);
  }
}

void PlatformRiot_enter_critical_section(Platform *super) {
  PlatformRiot *self = (PlatformRiot *)super;
  if (self->num_nested_critical_sections == 0) {
    self->irq_mask = irq_disable();
  }
  self->num_nested_critical_sections++;
}

void PlatformRiot_new_async_event(Platform *super) { mutex_unlock(&((PlatformRiot *)self)->lock); }

void Platform_ctor(Platform *super) {
  PlatformRiot *self = (PlatformRiot *)super;
  super->initialize = PlatformRiot_initialize;
  super->enter_critical_section = PlatformRiot_enter_critical_section;
  super->leave_critical_section = PlatformRiot_leave_critical_section;
  super->get_physical_time = PlatformRiot_get_physical_time;
  super->wait_until = PlatformRiot_wait_until;
  super->wait_for = PlatformRiot_wait_for;
  super->wait_until_interruptible_locked = PlatformRiot_wait_until_interruptible;
  super->new_async_event = PlatformRiot_new_async_event;
  self->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
