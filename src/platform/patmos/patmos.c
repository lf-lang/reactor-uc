#include "reactor-uc/platform/patmos/patmos.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <machine/rtc.h>
#include <machine/exceptions.h>

static PlatformPatmos platform;

void Platform_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }

lf_ret_t PlatformPatmos_initialize(Platform *super) {
  (void)self;
  return LF_OK;
}

instant_t PlatformPatmos_get_physical_time(Platform *super) {
  (void)self;
  return USEC(get_cpu_usecs());
}

lf_ret_t PlatformPatmos_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformPatmos *self = (PlatformPatmos *)super;
  self->async_event = false;
  super->leave_critical_section(super); // turing on interrupts

  instant_t now = super->get_physical_time(super);

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while ((now < wakeup_time) && !self->async_event);

  super->enter_critical_section(super);

  if (self->async_event) {
    self->async_event = false;
    return LF_ERR;
  } else {
    return LF_OK;
  }

  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  super->leave_critical_section(super);

  return LF_OK;
}

lf_ret_t PlatformPatmos_wait_until(Platform *super, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  instant_t now = super->get_physical_time(super);

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while (now < wakeup_time);
  return LF_OK;
}

lf_ret_t PlatformPatmos_wait_for(Platform *super, interval_t duration) {
  (void)self;
  if (duration <= 0) {
    return LF_OK;
  }

  instant_t now = super->get_physical_time(super);
  instant_t wakeup = now + duration;

  // Do busy sleep
  do {
    now = super->get_physical_time(super);
  } while ((now < wakeup));

  return LF_OK;
}

void PlatformPatmos_leave_critical_section(Platform *super) {
  (void)self;
  PlatformPatmos *self = (PlatformPatmos *)super;
  self->num_nested_critical_sections--;
  if (self->num_nested_critical_sections == 0) {
    intr_enable();
  } else if (self->num_nested_critical_sections < 0) {
    validate(false);
  }
}

void PlatformPatmos_enter_critical_section(Platform *super) {
  (void)self;
  PlatformPatmos *self = (PlatformPatmos *)super;
  if (self->num_nested_critical_sections == 0) {
    intr_disable();
  }
  self->num_nested_critical_sections++;
}

void PlatformPatmos_new_async_event(Platform *super) { ((PlatformPatmos *)self)->async_event = true; }

void Platform_ctor(Platform *super) {
  PlatformPatmos *self = (PlatformPatmos *)super;
  super->initialize = PlatformPatmos_initialize;
  super->enter_critical_section = PlatformPatmos_enter_critical_section;
  super->leave_critical_section = PlatformPatmos_leave_critical_section;
  super->get_physical_time = PlatformPatmos_get_physical_time;
  super->wait_until = PlatformPatmos_wait_until;
  super->wait_for = PlatformPatmos_wait_for;
  super->wait_until_interruptible_locked = PlatformPatmos_wait_until_interruptible;
  super->new_async_event = PlatformPatmos_new_async_event;
  self->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
