#include "reactor-uc/platform/riot.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static PlatformRiot platform;

static instant_t convert_usec_to_ns(uint32_t time) {
  return time * 1000;
}

uint32_t convert_ns_to_usec(instant_t time) {
  return time / 1000;
}

void PlatformPosix_initialize(Platform *self) {
  mutex_init(&((PlatformRiot*)self)->lock);
}

instant_t PlatformPosix_get_physical_time(Platform *self) {
  (void)self;

  return convert_usec_to_ns(ztimer_now(ZTIMER_USEC));
}

int PlatformPosix_wait_until_interruptable(Platform *self, instant_t wakeup_time) {
  (void)self;
  const struct timespec tspec = convert_ns_to_timespec(wakeup_time);
  int res = pthread_cond_timedwait(&cond, &lock, &tspec);
  if (res == 0) {
    return 1;
  } else if (res == ETIMEDOUT) {
    return 0;
  } else {
    return -1;
  }
}

int PlatformPosix_wait_until(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);

  uint32_t duration_usec = convert_ns_to_usec(sleep_duration);

  ztimer_sleep(ZTIMER_USEC, duration_usec);
  return 0;
}

void PlatformPosix_leave_critical_section(Platform *self) {
  mutex_unlock(&((PlatformRiot*)self)->lock);
}

void PlatformPosix_enter_critical_section(Platform *self) {
  mutex_lock(&((PlatformRiot*)self)->lock);
}

void PlatformPosix_new_async_event(Platform *self) {
  (void)self;
  int res = pthread_cond_signal(&cond);
  assert(res == 0);
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
