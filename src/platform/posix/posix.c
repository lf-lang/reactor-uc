#include "reactor-uc/platform/posix/posix.h"
#include "reactor-uc/logging.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

static PlatformPosix platform;

static instant_t convert_timespec_to_ns(struct timespec tp) { return ((instant_t)tp.tv_sec) * BILLION + tp.tv_nsec; }

void Platform_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }

// lf_exit should be defined in main.c and should call Environment_free, if not we provide an empty implementation here.
__attribute__((weak)) void lf_exit(void) { exit(0); }

static void handle_ctrlc(int sig) {
  (void)sig;
  lf_exit();
  exit(0);
}

static struct timespec convert_ns_to_timespec(instant_t time) {
  struct timespec tspec;
  tspec.tv_sec = time / BILLION;
  tspec.tv_nsec = (time % BILLION);
  return tspec;
}

lf_ret_t PlatformPosix_initialize(Platform *_self) {
  signal(SIGINT, handle_ctrlc);
  PlatformPosix *self = (PlatformPosix *)_self;
  if (pthread_mutex_init(&self->lock, NULL) != 0) {
    LF_ERR(PLATFORM, "Failed to initialize mutex");
    return LF_ERR;
  }
  if (pthread_cond_init(&self->cond, NULL) != 0) {
    LF_ERR(PLATFORM, "Failed to initialize cond var");
    return LF_ERR;
  }
  return LF_OK;
}

instant_t PlatformPosix_get_physical_time(Platform *self) {
  (void)self;
  struct timespec tspec;
  if (clock_gettime(CLOCK_REALTIME, (struct timespec *)&tspec) != 0) {
    throw("POSIX could not get physical time");
  }
  return convert_timespec_to_ns(tspec);
}

lf_ret_t PlatformPosix_wait_until_interruptible(Platform *_self, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Interruptable wait until %" PRId64, wakeup_time);
  PlatformPosix *self = (PlatformPosix *)_self;
  const struct timespec tspec = convert_ns_to_timespec(wakeup_time);
  int res = pthread_cond_timedwait(&self->cond, &self->lock, &tspec);
  if (res == 0) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    return LF_SLEEP_INTERRUPTED;
  } else if (res == ETIMEDOUT) {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_SLEEP_INTERRUPTED;
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

lf_ret_t PlatformPosix_wait_for(Platform *self, instant_t duration) {
  (void)self;
  const struct timespec tspec = convert_ns_to_timespec(duration);
  struct timespec remaining;
  int res = nanosleep((const struct timespec *)&tspec, (struct timespec *)&remaining);
  if (res == 0) {
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

lf_ret_t PlatformPosix_wait_until(Platform *self, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "wait until %" PRId64, wakeup_time);
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  LF_DEBUG(PLATFORM, "wait duration %" PRId64, sleep_duration);
  return PlatformPosix_wait_for(self, sleep_duration);
}

void PlatformPosix_leave_critical_section(Platform *_self) {
  PlatformPosix *self = (PlatformPosix *)_self;
  validaten(pthread_mutex_unlock(&self->lock));
  LF_DEBUG(PLATFORM, "Leave critical section");
}

void PlatformPosix_enter_critical_section(Platform *_self) {
  PlatformPosix *self = (PlatformPosix *)_self;
  validaten(pthread_mutex_lock(&self->lock));
  LF_DEBUG(PLATFORM, "Enter critical section");
}

void PlatformPosix_new_async_event(Platform *_self) {
  PlatformPosix *self = (PlatformPosix *)_self;
  validaten(pthread_cond_signal(&self->cond));
  LF_DEBUG(PLATFORM, "New async event");
}

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformPosix_enter_critical_section;
  self->leave_critical_section = PlatformPosix_leave_critical_section;
  self->get_physical_time = PlatformPosix_get_physical_time;
  self->wait_until = PlatformPosix_wait_until;
  self->wait_for = PlatformPosix_wait_for;
  self->initialize = PlatformPosix_initialize;
  self->wait_until_interruptible = PlatformPosix_wait_until_interruptible;
  self->new_async_event = PlatformPosix_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
