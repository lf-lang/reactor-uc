#include "reactor-uc/platform/posix/posix.h"
#include "reactor-uc/logging.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>

static PlatformPosix platform;

static instant_t convert_timespec_to_ns(struct timespec tp) {
  return ((instant_t)tp.tv_sec) * BILLION + tp.tv_nsec;
}

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

// lf_exit should be defined in main.c and should call Environment_free, if not we provide an empty implementation here.
__attribute__((weak)) void lf_exit(void) {
}

static void handle_signal(int sig) {
  (void)sig;
  printf("ERROR: Caught signal %d\n", sig);
  lf_exit();
  exit(1);
}

static struct timespec convert_ns_to_timespec(instant_t time) {
  struct timespec tspec;
  tspec.tv_sec = time / BILLION;
  tspec.tv_nsec = (time % BILLION);
  return tspec;
}

lf_ret_t PlatformPosix_initialize(Platform *super) {
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
  PlatformPosix *self = (PlatformPosix *)super;

  // Create a recursive mutex for nested critical sections.
  pthread_mutexattr_t attr;
  if (pthread_mutexattr_init(&attr) != 0) {
    LF_ERR(PLATFORM, "Failed to initialize mutex attributes");
    return LF_ERR;
  }
  if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
    LF_ERR(PLATFORM, "Failed to set mutex type to recursive");
    pthread_mutexattr_destroy(&attr);
    return LF_ERR;
  }
  if (pthread_mutex_init(&self->lock, &attr) != 0) {
    LF_ERR(PLATFORM, "Failed to initialize mutex");
    pthread_mutexattr_destroy(&attr);
    return LF_ERR;
  }
  pthread_mutexattr_destroy(&attr);

  if (pthread_cond_init(&self->cond, NULL) != 0) {
    LF_ERR(PLATFORM, "Failed to initialize cond var");
    return LF_ERR;
  }
  return LF_OK;
}

instant_t PlatformPosix_get_physical_time(Platform *super) {
  (void)super;
  struct timespec tspec;
  if (clock_gettime(CLOCK_REALTIME, (struct timespec *)&tspec) != 0) {
    throw("POSIX could not get physical time");
  }
  return convert_timespec_to_ns(tspec);
}

lf_ret_t PlatformPosix_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Interruptable wait until " PRINTF_TIME, wakeup_time);
  PlatformPosix *self = (PlatformPosix *)super;
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

lf_ret_t PlatformPosix_wait_for(Platform *super, instant_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  const struct timespec tspec = convert_ns_to_timespec(duration);
  struct timespec remaining;
  int res = nanosleep((const struct timespec *)&tspec, (struct timespec *)&remaining);
  if (res == 0) {
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

lf_ret_t PlatformPosix_wait_until(Platform *super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "wait until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "wait duration " PRINTF_TIME, sleep_duration);
  return PlatformPosix_wait_for(super, sleep_duration);
}

void PlatformPosix_leave_critical_section(Platform *super) {
  PlatformPosix *self = (PlatformPosix *)super;
  validaten(pthread_mutex_unlock(&self->lock));
  LF_DEBUG(PLATFORM, "Leave critical section");
}

void PlatformPosix_enter_critical_section(Platform *super) {
  PlatformPosix *self = (PlatformPosix *)super;
  validaten(pthread_mutex_lock(&self->lock));
  LF_DEBUG(PLATFORM, "Enter critical section");
}

void PlatformPosix_new_async_event(Platform *super) {
  PlatformPosix *self = (PlatformPosix *)super;
  validaten(pthread_cond_signal(&self->cond));
  LF_DEBUG(PLATFORM, "New async event");
}

void Platform_ctor(Platform *super) {
  super->enter_critical_section = PlatformPosix_enter_critical_section;
  super->leave_critical_section = PlatformPosix_leave_critical_section;
  super->get_physical_time = PlatformPosix_get_physical_time;
  super->wait_until = PlatformPosix_wait_until;
  super->wait_for = PlatformPosix_wait_for;
  super->initialize = PlatformPosix_initialize;
  super->wait_until_interruptible_locked = PlatformPosix_wait_until_interruptible;
  super->new_async_event = PlatformPosix_new_async_event;
}

Platform *Platform_new(void) {
  return (Platform *)&platform;
}
