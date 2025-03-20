#include "reactor-uc/logging.h"
#include "reactor-uc/platform/pico/pico.h"
#include <assert.h>
#include <pico/stdlib.h>
#include <pico/sync.h>
#include <stdbool.h>
#include <string.h>

static PlatformPico platform;

void Platform_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }

lf_ret_t PlatformPico_initialize(Platform *self) {
  PlatformPico *p = (PlatformPico *)self;
  stdio_init_all();
  // init sync structs
  critical_section_init(&p->crit_sec);
  sem_init(&p->sem, 0, 1);
  return LF_OK;
}

instant_t PlatformPico_get_physical_time(Platform *self) {
  (void)self;
  absolute_time_t now;
  now = get_absolute_time();
  return to_us_since_boot(now) * 1000;
}

lf_ret_t PlatformPico_wait_for(Platform *self, instant_t duration) {
  (void)self;
  if (duration <= 0)
    return LF_OK;
  int64_t sleep_duration_usec = duration / 1000;
  LF_DEBUG(PLATFORM, "Waiting duration %d usec", sleep_duration_usec);
  sleep_us(sleep_duration_usec);
  return LF_OK;
}

lf_ret_t PlatformPico_wait_until(Platform *self, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  return PlatformPico_wait_for(self, sleep_duration);
}

lf_ret_t PlatformPico_wait_until_interruptible(Platform *self, instant_t wakeup_time) {
  PlatformPico *p = (PlatformPico *)self;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);
  // time struct
  absolute_time_t target;

  // reset event semaphore
  sem_reset(&p->sem, 0);
  // create us boot wakeup time
  target = from_us_since_boot((uint64_t)(wakeup_time / 1000));
  // Enable interrupts.
  self->leave_critical_section(self);

  // blocked sleep
  // return on timeout or on processor event
  bool ret = sem_acquire_block_until(&p->sem, target);
  // Disable interrupts.
  self->enter_critical_section(self);

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
  LF_DEBUG(PLATFORM, "Leave critical section");
  p->num_nested_critical_sections--;
  if (p->num_nested_critical_sections == 0) {
    critical_section_exit(&p->crit_sec);
  } else if (p->num_nested_critical_sections < 0) {
    // Critical error, a bug in the runtime.
    validate(false);
  }
}

void PlatformPico_enter_critical_section(Platform *self) {
  PlatformPico *p = (PlatformPico *)self;
  LF_DEBUG(PLATFORM, "Enter critical section");

  // We only want to call critical_section_enter_blocking if we are outside a critical section.
  // Note that the reading of `p->num_nested_critical_sections` OUTSIDE of a critical section
  // will only work if we are using one of the pico cores.
  if (p->num_nested_critical_sections == 0) {
    critical_section_enter_blocking(&p->crit_sec);
  }
  p->num_nested_critical_sections++;
}

void PlatformPico_new_async_event(Platform *self) {
  LF_DEBUG(PLATFORM, "New async event");
  sem_release(&((PlatformPico *)self)->sem);
}

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformPico_enter_critical_section;
  self->leave_critical_section = PlatformPico_leave_critical_section;
  self->get_physical_time = PlatformPico_get_physical_time;
  self->wait_until = PlatformPico_wait_until;
  self->wait_for = PlatformPico_wait_for;
  self->initialize = PlatformPico_initialize;
  self->wait_until_interruptible = PlatformPico_wait_until_interruptible;
  self->new_async_event = PlatformPico_new_async_event;

  ((PlatformPico *)self)->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
