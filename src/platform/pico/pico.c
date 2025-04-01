#include "reactor-uc/logging.h"
#include "reactor-uc/platform/pico/pico.h"
#include <assert.h>
#include <pico/stdlib.h>
#include <pico/sync.h>
#include <stdbool.h>
#include <string.h>

static PlatformPico platform;

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

lf_ret_t PlatformPico_initialize(Platform *self) {
  PlatformPico *p = (PlatformPico *)self;
  // stdio_init_all();
  //  init sync structs
  critical_section_init(&p->crit_sec);
  sem_init(&p->sem, 0, 1);
  return LF_OK;
}

instant_t PlatformPico_get_physical_time(Platform *super) {
  (void)super;
  absolute_time_t now;
  now = get_absolute_time();
  return to_us_since_boot(now) * 1000;
}

lf_ret_t PlatformPico_wait_for(Platform *super, instant_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  int64_t sleep_duration_usec = duration / 1000;
  LF_DEBUG(PLATFORM, "Waiting duration %d usec", sleep_duration_usec);
  sleep_us(sleep_duration_usec);
  return LF_OK;
}

lf_ret_t PlatformPico_wait_until(Platform *super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  return PlatformPico_wait_for(super, sleep_duration);
}

lf_ret_t PlatformPico_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformPico *self = (PlatformPico *)super;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);
  // time struct
  absolute_time_t target;

  // reset event semaphore
  sem_reset(&p->sem, 0);
  // create us boot wakeup time
  target = from_us_since_boot((uint64_t)(wakeup_time / 1000));
  // Enable interrupts.
  // self->leave_critical_section(self);

  // blocked sleep
  // return on timeout or on processor event
  bool ret = sem_acquire_block_until(&self->sem, target);
  // Disable interrupts.
  // self->enter_critical_section(self);

  if (ret) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    return LF_SLEEP_INTERRUPTED;
  } else {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  }
}

void PlatformPico_leave_critical_section(Platform *self) {
  PlatformPico *p = (PlatformPico *)self;
  // LF_DEBUG(PLATFORM, "Leave critical section");
  critical_section_exit(&p->crit_sec);
}

void PlatformPico_enter_critical_section(Platform *self) {
  PlatformPico *p = (PlatformPico *)self;
  // LF_DEBUG(PLATFORM, "Enter critical section");
  critical_section_enter_blocking(&p->crit_sec);
}

void PlatformPico_new_async_event(Platform *self) {
  // LF_DEBUG(PLATFORM, "New async event");
  sem_release(&((PlatformPico *)self)->sem);
}

void Platform_ctor(Platform *super) {
  PlatformPico *self = (PlatformPico *)super;
  super->enter_critical_section = PlatformPico_enter_critical_section;
  super->leave_critical_section = PlatformPico_leave_critical_section;
  super->get_physical_time = PlatformPico_get_physical_time;
  super->wait_until = PlatformPico_wait_until;
  super->wait_for = PlatformPico_wait_for;
  super->initialize = PlatformPico_initialize;
  super->wait_until_interruptible_locked = PlatformPico_wait_until_interruptible;
  super->new_async_event = PlatformPico_new_async_event;

  self->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) {
  return (Platform *)&platform;
}
