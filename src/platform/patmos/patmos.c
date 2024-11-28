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

lf_ret_t PlatformPatmos_initialize(Platform *self) {
  (void)self;
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
  untyped_self->leave_critical_section(untyped_self); // turing on interrupts

  instant_t now = untyped_self->get_physical_time(untyped_self);

  // Do busy sleep
  do {
    now = untyped_self->get_physical_time(untyped_self);
  } while ((now < wakeup_time) && !self->async_event);
  
  untyped_self->enter_critical_section(untyped_self);

  if (self->async_event) {
    self->async_event = false;
    return LF_ERR;
  } else {
    return LF_OK;
  }


  interval_t sleep_duration = wakeup_time - untyped_self->get_physical_time(untyped_self);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  untyped_self->leave_critical_section(untyped_self);

  return LF_OK;
}

lf_ret_t PlatformRiot_wait_until(Platform *untyped_self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - untyped_self->get_physical_time(untyped_self);
  if (sleep_duration < 0) {
    return LF_OK;
  }
instant_t now = untyped_self->get_physical_time(untyped_self);

  // Do busy sleep
  do {
    now = untyped_self->get_physical_time(untyped_self);
  } while (now < wakeup_time);
  return LF_OK;
}

lf_ret_t PlatformRiot_wait_for(Platform *self, interval_t duration) {
  (void)self;
  if (duration <= 0) {
    return LF_OK;
  }

  instant_t now = self->get_physical_time(self);
  instant_t wakeup = now + duration;

  // Do busy sleep
  do {
    now = self->get_physical_time(self);
  } while ((now < wakeup));

  return LF_OK;
}

void PlatformRiot_leave_critical_section(Platform *self) {
  (void)self;
  intr_enable();
}

void PlatformRiot_enter_critical_section(Platform *self) {
  (void)self;
  intr_disable();
}

void PlatformPatmos_new_async_event(Platform *self) {
  ((PlatformPatmos*)self)->async_event = true;
}

void Platform_ctor(Platform *self) {
  self->initialize = PlatformPatmos_initialize;
  self->enter_critical_section = PlatformRiot_enter_critical_section;
  self->leave_critical_section = PlatformRiot_leave_critical_section;
  self->get_physical_time = PlatformRiot_get_physical_time;
  self->wait_until = PlatformRiot_wait_until;
  self->wait_for = PlatformRiot_wait_for;
  self->wait_until_interruptible = PlatformRiot_wait_until_interruptible;
  self->new_async_event = PlatformPatmos_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
