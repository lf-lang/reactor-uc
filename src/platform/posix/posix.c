#include "reactor-uc/platform/posix/posix.h"
#include "reactor-uc/logging.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

static PlatformPosix platform;

static instant_t convert_timespec_to_ns(struct timespec tp) { return ((instant_t)tp.tv_sec) * BILLION + tp.tv_nsec; }

static instant_t raw_realtime_ns(void) {
  struct timespec tspec;
  if (clock_gettime(CLOCK_REALTIME, (struct timespec*)&tspec) != 0) {
    throw("POSIX could not get physical time");
  }
  return convert_timespec_to_ns(tspec);
}

/**
 * @brief The instant this process treats as zero on its physical clock.
 *
 * POSIX reports CLOCK_REALTIME, which counts from 1970, while every embedded
 * platform here counts from boot. Counting from process start puts a host federate
 * on the same scale as the embedded federates, but it is not stable across
 * multiple federates on the same host.
 * LF_CLOCK_EPOCH_NS overrides it with an absolute CLOCK_REALTIME value in
 * nanoseconds. Several federates on one host launched a moment apart would
 * otherwise get slightly different zeros, and that difference behaves exactly
 * like clock-synchronisation error. A  launcher that exports one value to all
 * of them removes it. Ignored unless it parses and lies in the past.
 */
static instant_t clock_epoch = 0;
static pthread_once_t clock_epoch_once = PTHREAD_ONCE_INIT;

static void init_clock_epoch(void) {
  const instant_t now = raw_realtime_ns();
  const char* env = getenv("LF_CLOCK_EPOCH_NS");
  if (env != NULL && env[0] != '\0') {
    errno = 0;
    char* end = NULL;
    const long long parsed = strtoll(env, &end, 10);
    if (errno == 0 && end != NULL && *end == '\0' && parsed > 0 && (instant_t)parsed <= now) {
      clock_epoch = (instant_t)parsed;
      return;
    }
    LF_WARN(PLATFORM, "Ignoring unusable LF_CLOCK_EPOCH_NS=\"%s\"", env);
  }
  clock_epoch = now;
}

/**
 * @brief This process's zero instant, on the raw CLOCK_REALTIME scale.
 *
 * pthread_once because the network thread reads the clock too, and two threads
 * racing to initialise it could otherwise settle on different zeros.
 */
static instant_t clock_epoch_ns(void) {
  validaten(pthread_once(&clock_epoch_once, init_clock_epoch));
  return clock_epoch;
}

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
  return raw_realtime_ns() - clock_epoch_ns();
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

  // Convert time back to CLOCK_REALTIME, which is what pthread_cond_timedwait expects.
  // The epoch is added back to the wakeup time to get the absolute time on the CLOCK_REALTIME scale.
  const instant_t epoch = clock_epoch_ns();
  // If the wakeup time is too far in the future, we use FOREVER to avoid overflow.
  const instant_t deadline = (wakeup_time > FOREVER - epoch) ? FOREVER : wakeup_time + epoch;
  const struct timespec tspec = convert_ns_to_timespec(deadline);
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

  MUTEX_UNLOCK(self->mutex);
  return ret;
}

lf_ret_t PlatformPosix_wait_for(Platform* super, instant_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  const struct timespec tspec = convert_ns_to_timespec(duration);
  struct timespec remaining;
  const int res = nanosleep((const struct timespec*)&tspec, (struct timespec*)&remaining);
  if (res == 0) {
    return LF_OK;
  }

  return LF_ERR;
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
  // Save the clock's zero before any other thread exists.
  (void)clock_epoch_ns();
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