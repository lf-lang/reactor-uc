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

lf_ret_t PlatformRiot_initialize(Platform *self) {
  mutex_init(&((PlatformRiot *)self)->lock);
  mutex_lock(&((PlatformRiot *)self)->lock);
  return LF_OK;
}

instant_t PlatformRiot_get_physical_time(Platform *self) {
  (void)self;

  return USEC_TO_NSEC((ztimer64_now(ZTIMER64_USEC)));
}

lf_ret_t PlatformRiot_wait_until_interruptible(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  LF_DEBUG(PLATFORM, "Wait until interruptible for " PRINTF_TIME " ns", sleep_duration);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  self->leave_critical_section(self);
  int ret = ztimer64_mutex_lock_until(ZTIMER64_USEC, &((PlatformRiot *)self)->lock, NSEC_TO_USEC(wakeup_time));
  self->enter_critical_section(self);

  if (ret == 0) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    // the mutex was unlocked from IRQ (no timout occurred)
    return LF_SLEEP_INTERRUPTED;
  } else {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  }
}

lf_ret_t PlatformRiot_wait_until(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  ztimer64_sleep_until(ZTIMER64_USEC, NSEC_TO_USEC(wakeup_time));
  return LF_OK;
}

lf_ret_t PlatformRiot_wait_for(Platform *self, interval_t duration) {
  (void)self;
  if (duration <= 0)
    return LF_OK;
  ztimer64_sleep(ZTIMER64_USEC, NSEC_TO_USEC(duration));
  return LF_OK;
}

void PlatformRiot_leave_critical_section(Platform *self) {
  PlatformRiot *p = (PlatformRiot *)self;
  if (p->num_nested_critical_sections == 0) {
    irq_restore(p->irq_mask);
  }
  p->num_nested_critical_sections++;
}

void PlatformRiot_enter_critical_section(Platform *self) {
  PlatformRiot *p = (PlatformRiot *)self;
  p->num_nested_critical_sections--;
  if (p->num_nested_critical_sections == 0) {
    p->irq_mask = irq_disable();
  } else if (p->num_nested_critical_sections < 0) {
    validate(false);
  }
}

void PlatformRiot_new_async_event(Platform *self) { mutex_unlock(&((PlatformRiot *)self)->lock); }

void Platform_ctor(Platform *self) {
  self->initialize = PlatformRiot_initialize;
  self->enter_critical_section = PlatformRiot_enter_critical_section;
  self->leave_critical_section = PlatformRiot_leave_critical_section;
  self->get_physical_time = PlatformRiot_get_physical_time;
  self->wait_until = PlatformRiot_wait_until;
  self->wait_for = PlatformRiot_wait_for;
  self->wait_until_interruptible_locked = PlatformRiot_wait_until_interruptible;
  self->new_async_event = PlatformRiot_new_async_event;
  ((PlatformRiot *)self)->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
