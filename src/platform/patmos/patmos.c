#include "reactor-uc/platform/patmos/patmos.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <machine/rtc.h>
#include <machine/exceptions.h>

static PlatformPatmos platform;

#define USEC_TO_NSEC(usec) (usec * USEC(1))
#define NSEC_TO_USEC(nsec) (nsec / USEC(1))

void Platform_vprintf(const char *fmt, va_list args) { 
  vprintf(fmt, args); 
}

lf_ret_t PlatformRiot_initialize(Platform *self) {
  //TODO:
  return LF_OK;
}

instant_t PlatformRiot_get_physical_time(Platform *self) {
  (void)self;

  return USEC_TO_NSEC(get_cpu_usecs()); 
}

lf_ret_t PlatformRiot_wait_until_interruptible(Platform *untyped_self, instant_t wakeup_time) {
  PlatformPatmos* self = (PlatformPatmos*)untyped_self;
  self->async_event = false;
  self->leave_critical_section(self); // turing on interrupts

  instant_t now = self->get_physical_time(self);
  instant_t wakeup = now + sleep_duration;

  // Do busy sleep
  do {
    now = self->get_physical_time(self);
  } while ((now < wakeup) && !self->async_event);
  
  self->enter_critical_section(self)

  if (self->async_event) {
    self->async_event = false;
    return -1;
  } else {
    return 0;
  }


  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  self->leave_critical_section(self);
  int ret = ztimer64_mutex_lock_until(ZTIMER64_USEC, &((PlatformRiot *)self)->lock, NSEC_TO_USEC(wakeup_time));
  self->enter_critical_section(self);

  if (ret == 0) {
    // the mutex was unlocked from IRQ (no timeout occurred)
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

lf_ret_t PlatformRiot_wait_for(Platform *self, interval_t duration) {
  (void)self;
  if (duration <= 0) {
    return LF_OK;
  }

  instant_t now = self->get_physical_time(self);
  instant_t wakeup = now + sleep_duration;

  // Do busy sleep
  do {
    now = self->get_physical_time(self);
  } while ((now < wakeup));

  return LF_OK;
}

void PlatformRiot_leave_critical_section(Platform *self) {
  intr_enable();
  return 0;
}

void PlatformRiot_enter_critical_section(Platform *self) {
  intr_disable();
  return 0;
}

void PlatformPatmos_new_async_event(Platform *self) {
  ((PlatformPatmos*)self)->async_event = true;
}

void Platform_ctor(Platform *self) {
  self->initialize = PlatformRiot_initialize;
  self->enter_critical_section = PlatformRiot_enter_critical_section;
  self->leave_critical_section = PlatformRiot_leave_critical_section;
  self->get_physical_time = PlatformRiot_get_physical_time;
  self->wait_until = PlatformRiot_wait_until;
  self->wait_for = PlatformRiot_wait_for;
  self->wait_until_interruptible = PlatformRiot_wait_until_interruptible;
  self->new_async_event = PlatformRiot_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
