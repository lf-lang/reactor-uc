#include "reactor-uc/platform/riot/riot.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "reactor-uc/logging.h"

#include "irq.h"
#include "mutex.h"
#include "ztimer64.h"

#define USEC_TO_NSEC(usec) (usec * USEC(1))
#define NSEC_TO_USEC(nsec) (nsec / USEC(1))

static PlatformRiot platform;

void Platform_vprintf(const char* fmt, va_list args) { vprintf(fmt, args); }

instant_t PlatformRiot_get_physical_time(Platform* super) {
  (void)super;
  return USEC_TO_NSEC((ztimer64_now(ZTIMER64_USEC)));
}

lf_ret_t PlatformRiot_wait_until_interruptible(Platform* super, instant_t wakeup_time) {
  PlatformRiot* self = (PlatformRiot*)super;
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "Wait until interruptible for " PRINTF_TIME " ns", sleep_duration);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  int ret = ztimer64_mutex_lock_until(ZTIMER64_USEC, &self->lock, NSEC_TO_USEC(wakeup_time));

  if (ret == 0) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    // the mutex was unlocked from IRQ (no timout occurred)
    return LF_SLEEP_INTERRUPTED;
  } else {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  }
}

lf_ret_t PlatformRiot_wait_until(Platform* super, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  ztimer64_sleep_until(ZTIMER64_USEC, NSEC_TO_USEC(wakeup_time));
  return LF_OK;
}

lf_ret_t PlatformRiot_wait_for(Platform* super, interval_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  ztimer64_sleep(ZTIMER64_USEC, NSEC_TO_USEC(duration));
  return LF_OK;
}

void PlatformRiot_notify(Platform* super) {
  PlatformRiot* self = (PlatformRiot*)super;
  mutex_unlock(&self->lock);
}

void Platform_ctor(Platform* super) {
  PlatformRiot* self = (PlatformRiot*)super;
  super->get_physical_time = PlatformRiot_get_physical_time;
  super->wait_until = PlatformRiot_wait_until;
  super->wait_for = PlatformRiot_wait_for;
  super->wait_until_interruptible = PlatformRiot_wait_until_interruptible;
  super->notify = PlatformRiot_notify;

  mutex_init(&self->lock);
  mutex_lock(&self->lock);
}

Platform* Platform_new() { return &platform.super; }

void MutexRiot_lock(Mutex* super) {
  MutexRiot* self = (MutexRiot*)super;
  mutex_lock(&self->mutex);
}

void MutexRiot_unlock(Mutex* super) {
  MutexRiot* self = (MutexRiot*)super;
  mutex_unlock(&self->mutex);
}

void Mutex_ctor(Mutex* super) {
  MutexRiot* self = (MutexRiot*)super;
  super->lock = MutexRiot_lock;
  super->unlock = MutexRiot_unlock;
  mutex_init(&self->mutex);
}