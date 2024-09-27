#include "reactor-uc/platform/posix.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static PlatformPosix platform;
// TODO: Put these on the platform struct?
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static instant_t convert_timespec_to_ns(struct timespec tp) { return ((instant_t)tp.tv_sec) * BILLION + tp.tv_nsec; }

static struct timespec convert_ns_to_timespec(instant_t time) {
  struct timespec tspec;
  tspec.tv_sec = time / BILLION;
  tspec.tv_nsec = (time % BILLION);
  return tspec;
}

void PlatformPosix_initialize(Platform *self) { (void)self; }

instant_t PlatformPosix_get_physical_time(Platform *self) {
  (void)self;
  struct timespec tspec;
  if (clock_gettime(CLOCK_REALTIME, (struct timespec *)&tspec) != 0) {
    assert(false); // TODO: How should this be handled?
  }
  return convert_timespec_to_ns(tspec);
}

int PlatformPosix_wait_until_interruptable(Platform *self, instant_t wakeup_time) {
  (void)self;
  const struct timespec tspec = convert_ns_to_timespec(wakeup_time);
  int res = pthread_cond_timedwait(&cond, &lock, &tspec);
  if (res == 0) {
    return SLEEP_INTERRUPTED;
  } else if (res == ETIMEDOUT) {
    return SLEEP_COMPLETED;
  } else {
    return SLEEP_ERROR;
  }
}

int PlatformPosix_wait_until(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);

  const struct timespec tspec = convert_ns_to_timespec(sleep_duration);
  struct timespec remaining;
  int res = nanosleep((const struct timespec *)&tspec, (struct timespec *)&remaining);
  if (res != 0) {
    printf("error=%s\n", strerror(errno));
    assert(false);
  }
  return 0;
}

void PlatformPosix_leave_critical_section(Platform *self) {
  (void)self;
  int res = pthread_mutex_unlock(&lock);
  assert(res == 0);
}

void PlatformPosix_enter_critical_section(Platform *self) {
  (void)self;
  int res = pthread_mutex_lock(&lock);
  assert(res == 0);
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

Platform *Platform_new(void) { return (Platform *)&platform; }
