#include "reactor-uc/platform/riot.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ztimer.h"
#include "mutex.h"
#include "irq.h"

static PlatformRiot platform;

static instant_t convert_usec_to_ns(uint32_t time) {
  return time * 1000;
}

uint32_t convert_ns_to_usec(instant_t time) {
  return time / 1000;
}

void PlatformPosix_initialize(Platform *self) {
  mutex_init(&((PlatformRiot*)self)->lock);
  mutex_lock(&((PlatformRiot*)self)->lock);
}

instant_t PlatformPosix_get_physical_time(Platform *self) {
  (void)self;

  return convert_usec_to_ns(ztimer_now(ZTIMER_USEC));
}

WaitUntilReturn PlatformPosix_wait_until_interruptable(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  uint32_t timeout = convert_ns_to_usec(sleep_duration);

  int ret = ztimer_mutex_lock_timeout(ZTIMER_USEC, &((PlatformRiot*)self)->lock, timeout);

  if (ret == 0) {
    // the mutex was unlocked from IRQ (no timout occurred)
    return SLEEP_INTERRUPTED;
  } else {
    return SLEEP_COMPLETED;
  }
}

int PlatformPosix_wait_until(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  uint32_t duration_usec = convert_ns_to_usec(sleep_duration);

  ztimer_sleep(ZTIMER_USEC, duration_usec);
  return 0;
}

void PlatformPosix_leave_critical_section(Platform *self) {
  (void)self;
  irq_enable();
}

void PlatformPosix_enter_critical_section(Platform *self) {
  (void)self;
  irq_disable();
}

void PlatformPosix_new_async_event(Platform *self) {
  mutex_unlock(&((PlatformRiot*)self)->lock);
}

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformPosix_enter_critical_section;
  self->leave_critical_section = PlatformPosix_leave_critical_section;
  self->get_physical_time = PlatformPosix_get_physical_time;
  self->wait_until = PlatformPosix_wait_until;
  self->initialize = PlatformPosix_initialize;
  self->wait_until_interruptable = PlatformPosix_wait_until_interruptable;
  self->new_async_event = PlatformPosix_new_async_event;
}

Platform *Platform_new(void) {
  return (Platform *)&platform;
}
