#include "reactor-uc/platform/zephyr/zephyr.h"
#include "reactor-uc/logging.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "zephyr/sys/time_units.h"

static PlatformZephyr platform;

lf_ret_t PlatformZephyr_initialize(Platform *self) {
  int ret = k_sem_init(&((PlatformZephyr *)self)->sem, 0, 1);
  if (ret == 0) {
    LF_ERR(PLATFORM, "Failed to initialize semaphore");
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

// FIXME: This only gives us msec level accuracy, but using other clocks
// doesnt work well with QEMU currently.
instant_t PlatformZephyr_get_physical_time(Platform *self) {
  (void)self;
  return k_uptime_get() * MSEC(1);
}

lf_ret_t PlatformZephyr_wait_until(Platform *self, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until %" PRId64, wakeup_time);
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  int32_t sleep_duration_usec = sleep_duration / 1000;
  LF_DEBUG(PLATFORM, "Waiting duration %d usec", sleep_duration_usec);
  int ret = k_usleep(sleep_duration_usec);
  if (ret == 0) {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  } else {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    return LF_SLEEP_INTERRUPTED;
  }
}

lf_ret_t PlatformZephyr_wait_until_interruptable(Platform *self, instant_t wakeup_time) {
  PlatformZephyr *p = (PlatformZephyr *)self;
  LF_DEBUG(PLATFORM, "Wait until interruptable %" PRId64, wakeup_time);
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  LF_DEBUG(PLATFORM, "Wait until interruptable for %" PRId64, wakeup_time);
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
  } else if (ret == -EAGAIN) {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  } else {
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
  self->initialize = PlatformZephyr_initialize;
  self->wait_until_interruptable = PlatformZephyr_wait_until_interruptable;
  self->new_async_event = PlatformZephyr_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
