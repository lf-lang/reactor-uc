#include "reactor-uc/logging.h"
#include "reactor-uc/platform/pico/pico.h"
#include <assert.h>
#include <pico/stdlib.h>
#include <pico/sync.h>
#include <stdbool.h>
#include <string.h>

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

instant_t PlatformPico_get_physical_time(Platform *super) {
  (void)super;
  absolute_time_t now;
  now = get_absolute_time();
  return to_us_since_boot(now) * 1000;
}

lf_ret_t PlatformPico_wait_for(Platform *super, instant_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  int64_t sleep_duration_usec = duration / 1000;
  LF_DEBUG(PLATFORM, "Waiting duration %d usec", sleep_duration_usec);
  sleep_us(sleep_duration_usec);
  return LF_OK;
}


lf_ret_t PlatformPico_wait_until(Platform *super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  return PlatformPico_wait_for(super, sleep_duration);
}

lf_ret_t PlatformPico_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformPico *self = (PlatformPico *)super;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);
  // time struct
  absolute_time_t target;

  // reset event semaphore
  sem_reset(&self->sem, 0);
  // create us boot wakeup time
  target = from_us_since_boot((uint64_t)(wakeup_time / 1000));

  // blocked sleep
  // return on timeout or on processor event
  bool ret = sem_acquire_block_until(&self->sem, target);

  if (ret) {
    LF_DEBUG(PLATFORM, "Wait until interrupted");
    return LF_SLEEP_INTERRUPTED;
  } else {
    LF_DEBUG(PLATFORM, "Wait until completed");
    return LF_OK;
  }
}

void PlatformPico_notify(Platform *super) {
  PlatformPico *self = (PlatformPico *)super;
  LF_DEBUG(PLATFORM, "New async event");
  sem_release(&self->sem);
}


void Platform_ctor(Platform *super) {
  PlatformPico *self = (PlatformPico *)super;
  super->get_physical_time = PlatformPico_get_physical_time;
  super->wait_until = PlatformPico_wait_until;
  super->wait_for = PlatformPico_wait_for;
  super->wait_until_interruptible = PlatformPico_wait_until_interruptible;
  super->notify = PlatformPico_notify;

  stdio_init_all();
  sem_init(&self->sem, 0, 1);
}

void MutexPico_unlock(Mutex *super) {
  MutexPico *self = (MutexPico *)super;
  critical_section_exit(&self->crit_sec);
}

void MutexPico_lock(Mutex *super) {
  MutexPico *self = (MutexPico *)super;
  critical_section_enter_blocking(&self->crit_sec);
}

void Mutex_ctor(Mutex *super) {
  static unsigned lock_num_cnt_1= 0;
  static unsigned lock_num_cnt_2= 0;
  unsigned core_num = get_core_num();

  unsigned lock_num;
  if (core_num == 0) {
    lock_num =lock_num_cnt_1++;
  } else if (core_num == 1) {
    lock_num =lock_num_cnt_2++;
  } else {
    validate(false);
  }

  MutexPico *self = (MutexPico *)super;
  super->lock = MutexPico_lock;
  super->unlock = MutexPico_unlock;
  critical_section_init_with_lock_num(&self->crit_sec, lock_num++);
}