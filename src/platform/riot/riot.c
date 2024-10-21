#include "reactor-uc/platform/riot/riot.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

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
  if (sleep_duration < 0) {
    return LF_OK;
  }

  self->leave_critical_section(self);
  int ret = ztimer64_mutex_lock_until(ZTIMER64_USEC, &((PlatformRiot *)self)->lock, NSEC_TO_USEC(wakeup_time));
  self->enter_critical_section(self);

  if (ret == 0) {
    // the mutex was unlocked from IRQ (no timout occurred)
    return LF_SLEEP_INTERRUPTED;
  } else {
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

void PlatformRiot_leave_critical_section(Platform *self) {
  PlatformRiot *p = (PlatformRiot *)self;
  p->irq_mask = irq_disable();
}

void PlatformRiot_enter_critical_section(Platform *self) {
  PlatformRiot *p = (PlatformRiot *)self;
  irq_restore(p->irq_mask);
}

void PlatformRiot_new_async_event(Platform *self) { mutex_unlock(&((PlatformRiot *)self)->lock); }

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformRiot_enter_critical_section;
  self->leave_critical_section = PlatformRiot_leave_critical_section;
  self->get_physical_time = PlatformRiot_get_physical_time;
  self->wait_until = PlatformRiot_wait_until;
  self->initialize = PlatformRiot_initialize;
  self->wait_until_interruptible = PlatformRiot_wait_until_interruptible;
  self->new_async_event = PlatformRiot_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
