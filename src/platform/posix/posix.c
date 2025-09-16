#define _GNU_SOURCE
#include "reactor-uc/platform/posix/posix.h"
#include "reactor-uc/logging.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <sched.h>

static PlatformPosix platform;

extern int get_priority_value(interval_t);

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
  lf_ret_t ret;
  PlatformPosix *self = (PlatformPosix *)super;
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

void PlatformPosix_notify(Platform *super) {
  PlatformPosix *self = (PlatformPosix *)super;
  MUTEX_LOCK(self->mutex);
  self->new_async_event = true;
  validaten(pthread_cond_signal(&self->cond));
  MUTEX_UNLOCK(self->mutex);

  LF_DEBUG(PLATFORM, "New async event");
}

lf_ret_t PlatformPosix_set_thread_priority(interval_t rel_deadline) {
  if (LF_THREAD_POLICY == LF_SCHED_FAIR) {
    // Not setting the priority if the scheduling policy is non-real-time
    return LF_OK;
  }

  // TCP thread has got the highest priority,
  // the main thread when it sleeps has got the second highest priority
  int prio;
  if (rel_deadline == LF_SLEEP_PRIORITY) {
    prio = 98;
  } else if (rel_deadline == LF_TCP_THREAD_PRIORITY) {
    prio = 99;
  } else if (rel_deadline == NEVER) {
    prio = 1;
  } else {
    prio = get_priority_value(rel_deadline);
  }

  // using pthread's APIs to set current thread priority
  if (pthread_setschedprio(pthread_self(), prio) != 0) {
    return LF_ERR;
  }

  LF_DEBUG(PLATFORM, "Set thread priority to %d", prio);

  return LF_OK;
}

int PlatformPosix_get_thread_priority() {
  if (LF_THREAD_POLICY == LF_SCHED_FAIR) {
    // Not returning the priority if the scheduling policy is non-real-time
    return -1;
  }

  int posix_policy;
  int ret;
  struct sched_param schedparam;
  ret = pthread_getschedparam(pthread_self(), &posix_policy, &schedparam);
  if (ret != 0) {
    throw("Could not get the schedparam data structure.");
  }

  int prio = schedparam.sched_priority;
  LF_DEBUG(PLATFORM, "Current thread has priority %d", prio);

  return prio;
}

lf_ret_t PlatformPosix_set_scheduling_policy() {
  int posix_policy;
  int ret;
  struct sched_param schedparam;

  // Get the current scheduling policy
  ret = pthread_getschedparam(pthread_self(), &posix_policy, &schedparam);
  if (ret != 0) {
    throw("Could not get the schedparam data structure.");
  }

  // Update the policy, and initially set the priority to max.
  // The priority value is later updated. Initializing it
  // is just to avoid code duplication.
  switch (LF_THREAD_POLICY) {
    case LF_SCHED_FAIR:
      LF_DEBUG(PLATFORM, "Setting scheduling policy to fair");
      posix_policy = SCHED_OTHER;
      schedparam.sched_priority = 0;
      break;
    case LF_SCHED_TIMESLICE:
      LF_DEBUG(PLATFORM, "Setting scheduling policy to timeslice");
      posix_policy = SCHED_RR;
      schedparam.sched_priority = sched_get_priority_max(SCHED_RR);
      break;
    case LF_SCHED_PRIORITY:
      LF_DEBUG(PLATFORM, "Setting scheduling policy to priority");
      posix_policy = SCHED_FIFO;
      schedparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
      break;
    default:
      throw("Unknown scheduling policy. Please select one of LF_SCHED_FAIR, LF_SCHED_TIMESLICE, or LF_SCHED_PRIORITY.");
    }

  ret = pthread_setschedparam(pthread_self(), posix_policy, &schedparam);
  if (ret != 0) {
    throw("Could not set the selected scheduling policy. Try launching the program with sudo rights.");
  }

  return LF_OK;
}

lf_ret_t PlatformPosix_set_core_affinity() {
  int ret;
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);

  int n_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
  
  if (LF_NUMBER_OF_CORES <= 0) {
    // Nothing to do, use all cores
    LF_DEBUG(PLATFORM, "Using all cores");
    return LF_OK;
  }

  if (LF_NUMBER_OF_CORES > n_cores) {
    // Nothing to do, use all cores
    char err_msg[100];
    sprintf(err_msg, "Cannot use %d cores, only %d cores are available.", LF_NUMBER_OF_CORES, n_cores);
    throw(err_msg);
  }

  // Setting the CPUs where the current thread will run, starting from n_cores - 1
  for (int idx = n_cores - 1; idx >= n_cores - LF_NUMBER_OF_CORES; idx--) {
    CPU_SET(idx, &cpu_set);
    LF_DEBUG(PLATFORM, "Using core %d", idx);
  }

  ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
  if (ret != 0) {
    throw("Could not set the selected core affinity.");
  }

  return LF_OK;
}

void Platform_ctor(Platform *super) {
  PlatformPosix *self = (PlatformPosix *)super;
  super->get_physical_time = PlatformPosix_get_physical_time;
  super->wait_until = PlatformPosix_wait_until;
  super->wait_for = PlatformPosix_wait_for;
  super->wait_until_interruptible = PlatformPosix_wait_until_interruptible;
  super->notify = PlatformPosix_notify;
  super->set_thread_priority = PlatformPosix_set_thread_priority;
  super->set_scheduling_policy = PlatformPosix_set_scheduling_policy;
  super->set_core_affinity = PlatformPosix_set_core_affinity;
  super->get_thread_priority = PlatformPosix_get_thread_priority;

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
  Mutex_ctor(&self->mutex.super);

  // Initialize the condition variable used for sleeping.
  validaten(pthread_cond_init(&self->cond, NULL));
}

Platform *Platform_new() {
  return &platform.super;
}

void MutexPosix_lock(Mutex *super) {
  MutexPosix *self = (MutexPosix *)super;
  validaten(pthread_mutex_lock(&self->lock));
}

void MutexPosix_unlock(Mutex *super) {
  MutexPosix *self = (MutexPosix *)super;
  validaten(pthread_mutex_unlock(&self->lock));
}

void Mutex_ctor(Mutex *super) {
  MutexPosix *self = (MutexPosix *)super;
  super->lock = MutexPosix_lock;
  super->unlock = MutexPosix_unlock;
  validaten(pthread_mutex_init(&self->lock, NULL));
}