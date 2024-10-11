#include "reactor-uc/platform/posix/posix.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

static PlatformPosix platform;

static instant_t convert_timespec_to_ns(struct timespec tp) { return ((instant_t)tp.tv_sec) * BILLION + tp.tv_nsec; }

static struct timespec convert_ns_to_timespec(instant_t time) {
  struct timespec tspec;
  tspec.tv_sec = time / BILLION;
  tspec.tv_nsec = (time % BILLION);
  return tspec;
}

lf_ret_t PlatformPosix_initialize(Platform *_self) {
  PlatformPosix *self = (PlatformPosix *)_self;
  if (pthread_mutex_init(&self->lock, NULL) != 0) {
    return LF_ERR;
  }
  if (pthread_cond_init(&self->cond, NULL) != 0) {
    return LF_ERR;
  }
  return LF_OK;
}

instant_t PlatformPosix_get_physical_time(Platform *self) {
  (void)self;
  struct timespec tspec;
  if (clock_gettime(CLOCK_REALTIME, (struct timespec *)&tspec) != 0) {
    validate(false);
  }
  return convert_timespec_to_ns(tspec);
}

lf_ret_t PlatformPosix_wait_until_interruptable(Platform *_self, instant_t wakeup_time) {
  PlatformPosix *self = (PlatformPosix *)_self;
  const struct timespec tspec = convert_ns_to_timespec(wakeup_time);
  int res = pthread_cond_timedwait(&self->cond, &self->lock, &tspec);
  if (res == 0) {
    return LF_SLEEP_INTERRUPTED;
  } else if (res == ETIMEDOUT) {
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

lf_ret_t PlatformPosix_wait_until(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);

  const struct timespec tspec = convert_ns_to_timespec(sleep_duration);
  struct timespec remaining;
  int res = nanosleep((const struct timespec *)&tspec, (struct timespec *)&remaining);
  if (res == 0) {
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

void PlatformPosix_leave_critical_section(Platform *_self) {
  PlatformPosix *self = (PlatformPosix *)_self;
  validaten(pthread_mutex_unlock(&self->lock));
}

void PlatformPosix_enter_critical_section(Platform *_self) {
  PlatformPosix *self = (PlatformPosix *)_self;
  validaten(pthread_mutex_lock(&self->lock));
}

void PlatformPosix_new_async_event(Platform *_self) {
  PlatformPosix *self = (PlatformPosix *)_self;
  validaten(pthread_cond_signal(&self->cond));
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

Platform *Platform_new(void) { return (Platform *)&platform; }
