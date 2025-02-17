#include "reactor-uc/platform/zephyr/zephyr.h"
#include "reactor-uc/logging.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "zephyr/sys/time_units.h"
#include <zephyr/fatal_types.h>

static PlatformZephyr platform;

void Platform_vprintf(const char *fmt, va_list args) { vprintk(fmt, args); }

// Catch kernel panics from Zephyr
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf) {
  (void)esf;
  LF_ERR(PLATFORM, "Zephyr kernel panic reason=%d", reason);
  throw("Zephyr kernel panic");
}

lf_ret_t PlatformZephyr_initialize(Platform *self) {
  int ret = k_sem_init(&((PlatformZephyr *)self)->sem, 0, 1);
  if (ret == 0) {
    return LF_OK;
  } else {
    LF_ERR(PLATFORM, "Failed to initialize semaphore");
    return LF_ERR;
  }
}

// TODO: k_uptime_get has msec-level accuracy. It can be worth investigating other kernel APIs.
instant_t PlatformZephyr_get_physical_time(Platform *self) {
  (void)self;
  return k_uptime_get() * MSEC(1);
}

// TODO: We can only sleep for a maximum of 2^31-1 microseconds. Investigate if we can sleep for longer.
lf_ret_t PlatformZephyr_wait_for(Platform *self, interval_t duration) {
  (void)self;
  if (duration <= 0)
    return LF_OK;
  int32_t sleep_duration_usec = duration / 1000;
  LF_DEBUG(PLATFORM, "Waiting duration %d usec", sleep_duration_usec);
  k_usleep(sleep_duration_usec);
  return LF_OK;
}

lf_ret_t PlatformZephyr_wait_until(Platform *self, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  return PlatformZephyr_wait_for(self, sleep_duration);
}

lf_ret_t PlatformZephyr_wait_until_interruptible(Platform *self, instant_t wakeup_time) {
  PlatformZephyr *p = (PlatformZephyr *)self;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  LF_DEBUG(PLATFORM, "Wait until interruptible for " PRINTF_TIME, wakeup_time);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  k_sem_reset(&p->sem);

  self->leave_critical_section(self);
  int ret = k_sem_take(&p->sem, K_NSEC(sleep_duration));
  self->enter_critical_section(self);

  if (ret == 0) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    return LF_SLEEP_INTERRUPTED;
  } else if (ret == -EAGAIN ||
             ret == -EBUSY) { // EAGAIN means that we timed out. EBUSY means we passed in a wait of zero.
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  } else {
    LF_ERR(PLATFORM, "Wait until failed with %d", ret);
    return LF_ERR;
  }
}

void PlatformZephyr_leave_critical_section(Platform *self) {
  PlatformZephyr *p = (PlatformZephyr *)self;
  LF_DEBUG(PLATFORM, "Leave critical section");
  irq_unlock(p->irq_mask);
}

void PlatformZephyr_enter_critical_section(Platform *self) {
  PlatformZephyr *p = (PlatformZephyr *)self;
  LF_DEBUG(PLATFORM, "Enter critical section");
  p->irq_mask = irq_lock();
}

void PlatformZephyr_new_async_event(Platform *self) {
  LF_DEBUG(PLATFORM, "New async event");
  k_sem_give(&((PlatformZephyr *)self)->sem);
}

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformZephyr_enter_critical_section;
  self->leave_critical_section = PlatformZephyr_leave_critical_section;
  self->get_physical_time = PlatformZephyr_get_physical_time;
  self->wait_until = PlatformZephyr_wait_until;
  self->wait_for = PlatformZephyr_wait_for;
  self->initialize = PlatformZephyr_initialize;
  self->wait_until_interruptible = PlatformZephyr_wait_until_interruptible;
  self->new_async_event = PlatformZephyr_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
