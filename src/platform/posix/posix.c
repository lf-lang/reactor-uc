#include "reactor-uc/platform/posix/posix.h"
#include "reactor-uc/logging.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

static PlatformPosix platform;

static instant_t convert_timespec_to_ns(struct timespec tp) { return ((instant_t)tp.tv_sec) * BILLION + tp.tv_nsec; }

void Platform_vprintf(const char* fmt, va_list args) { vprintf(fmt, args); }

// lf_exit should be defined in main.c and should call Environment_free, if not we provide an empty implementation here.
__attribute__((weak)) void lf_exit(void) {}

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

instant_t PlatformPosix_get_physical_time(Platform* super) {
  (void)super;
  struct timespec tspec;
  if (clock_gettime(CLOCK_REALTIME, (struct timespec*)&tspec) != 0) {
    throw("POSIX could not get physical time");
  }
  return convert_timespec_to_ns(tspec);
}

lf_ret_t PlatformPosix_wait_until_interruptible(Platform* super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Interruptable wait until " PRINTF_TIME, wakeup_time);
  lf_ret_t ret;
  PlatformPosix* self = (PlatformPosix*)super;
  MUTEX_LOCK(self->mutex);

  if (self->new_async_event) {
    self->new_async_event = false;
    MUTEX_UNLOCK(self->mutex);
    return LF_SLEEP_INTERRUPTED;
  }

  const struct timespec tspec = convert_ns_to_timespec(wakeup_time);
  int res = pthread_cond_timedwait(&self->cond, &self->mutex.lock, &tspec);
  if (res == 0) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    ret = LF_SLEEP_INTERRUPTED;
  } else if (res == ETIMEDOUT) {
    LF_DEBUG(PLATFORM, "Wait until completed");
    ret = LF_OK;
  } else {
    validate(false);
  }

  ret = self->new_async_event ? LF_SLEEP_INTERRUPTED : ret;
  self->new_async_event = false;
  MUTEX_UNLOCK(self->mutex);
  return ret;
}

lf_ret_t PlatformPosix_wait_for(Platform* super, instant_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  const struct timespec tspec = convert_ns_to_timespec(duration);
  struct timespec remaining;
  int res = nanosleep((const struct timespec*)&tspec, (struct timespec*)&remaining);
  if (res == 0) {
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

lf_ret_t PlatformPosix_wait_until(Platform* super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "wait until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "wait duration " PRINTF_TIME, sleep_duration);
  return PlatformPosix_wait_for(super, sleep_duration);
}

void PlatformPosix_notify(Platform* super) {
  PlatformPosix* self = (PlatformPosix*)super;
  MUTEX_LOCK(self->mutex);
  self->new_async_event = true;
  validaten(pthread_cond_signal(&self->cond));
  MUTEX_UNLOCK(self->mutex);

  LF_DEBUG(PLATFORM, "New async event");
}

void Platform_ctor(Platform* super) {
  PlatformPosix* self = (PlatformPosix*)super;
  super->get_physical_time = PlatformPosix_get_physical_time;
  super->wait_until = PlatformPosix_wait_until;
  super->wait_for = PlatformPosix_wait_for;
  super->wait_until_interruptible = PlatformPosix_wait_until_interruptible;
  super->notify = PlatformPosix_notify;

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
  Mutex_ctor(&self->mutex.super);

  // Initialize the condition variable used for sleeping.
  validaten(pthread_cond_init(&self->cond, NULL));
}

Platform* Platform_new() { return &platform.super; }

void MutexPosix_lock(Mutex* super) {
  MutexPosix* self = (MutexPosix*)super;
  validaten(pthread_mutex_lock(&self->lock));
}

void MutexPosix_unlock(Mutex* super) {
  MutexPosix* self = (MutexPosix*)super;
  validaten(pthread_mutex_unlock(&self->lock));
}

void Mutex_ctor(Mutex* super) {
  MutexPosix* self = (MutexPosix*)super;
  super->lock = MutexPosix_lock;
  super->unlock = MutexPosix_unlock;
  validaten(pthread_mutex_init(&self->lock, NULL));
}