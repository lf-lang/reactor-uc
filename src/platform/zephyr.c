#include "reactor-uc/platform/zephyr.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "zephyr/sys/time_units.h"

static PlatformZephyr platform;

lf_ret_t PlatformZephyr_initialize(Platform *self) {
  int ret = k_sem_init(&((PlatformZephyr *)self)->sem, 0, 1);
  if (ret == 0) {
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
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  int32_t sleep_duration_usec = sleep_duration / 1000;
  int ret = k_usleep(sleep_duration_usec);
  if (ret == 0) {
    return LF_OK;
  } else {
    return LF_SLEEP_INTERRUPTED;
  }
}

lf_ret_t PlatformZephyr_wait_until_interruptable(Platform *self, instant_t wakeup_time) {
  PlatformZephyr *p = (PlatformZephyr *)self;
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  k_sem_reset(&p->sem);

  validaten(self->leave_critical_section(self));
  int ret = k_sem_take(&p->sem, K_NSEC(sleep_duration));
  validaten(self->enter_critical_section(self));

  if (ret == 0) {
    return LF_SLEEP_INTERRUPTED;
  } else if (ret == -EAGAIN) {
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

lf_ret_t PlatformZephyr_leave_critical_section(Platform *self) {
  PlatformZephyr *p = (PlatformZephyr *)self;
  irq_unlock(p->irq_mask);
  return LF_OK;
}

lf_ret_t PlatformZephyr_enter_critical_section(Platform *self) {
  PlatformZephyr *p = (PlatformZephyr *)self;
  p->irq_mask = irq_lock();
  return LF_OK;
}

lf_ret_t PlatformZephyr_new_async_event(Platform *self) {
  k_sem_give(&((PlatformZephyr *)self)->sem);
  return LF_OK;
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
