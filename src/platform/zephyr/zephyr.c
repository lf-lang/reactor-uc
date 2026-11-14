#include "reactor-uc/platform/zephyr/zephyr.h"
#include "reactor-uc/logging.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/fatal_types.h>

// Make sure k_timeout_t (passed to k_sleep and k_sem_take) stores ticks as uint64 (default: true),
// otherwise the maximum timeout time is too low at high tick rates:
// ~5 days at 10k, ~12 hours at 100k, ~71 minutes at 1M ticks per second
#ifndef CONFIG_TIMEOUT_64BIT
#error "CONFIG_TIMEOUT_64BIT required"
#endif

static PlatformZephyr platform;

void Platform_vprintf(const char* fmt, va_list args) { vprintk(fmt, args); }

// Catch kernel panics from Zephyr
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf* esf) {
  (void)esf;
  LF_ERR(PLATFORM, "Zephyr kernel panic reason=%d", reason);
  throw("Zephyr kernel panic");
}

instant_t PlatformZephyr_get_physical_time(Platform* super) {
  (void)super;
#ifdef CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER
  // use high resolution 64-bit hardware counter if available
  return NSEC(k_cyc_to_ns_floor64(k_cycle_get_64()));
#else
  // fallback to kernel ticks (less resolution)
  return NSEC(k_ticks_to_ns_floor64(k_uptime_ticks()));
#endif
}

// The sleep time resolution depends on the kernel tick rate (CONFIG_SYS_CLOCK_TICKS_PER_SEC).
// A sleep time shorter than the time per tick will be rounded to the next tick.
// It defaults to 10000 for CONFIG_TICKLESS_KERNEL (default: true) -> 100us per tick
// or 100 for non-tickless kernel (or CONFIG_QEMU_TARGET, CONFIG_SOC_POSIX) -> 10ms per tick.
lf_ret_t PlatformZephyr_wait_for(Platform* super, interval_t duration) {
  (void)super;
  if (duration <= 0)
    return LF_OK;
  LF_DEBUG(PLATFORM, "Waiting duration " PRINTF_TIME " nsec", duration);

  int32_t ms_remaining = k_sleep(K_NSEC(duration));
  while (ms_remaining > 0) { // only if awoken early (should not happen)
    ms_remaining = k_sleep(K_MSEC(ms_remaining));
  }
  return LF_OK;
}

lf_ret_t PlatformZephyr_wait_until(Platform* super, instant_t wakeup_time) {
  LF_DEBUG(PLATFORM, "Waiting until " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  return PlatformZephyr_wait_for(super, sleep_duration);
}

lf_ret_t PlatformZephyr_wait_until_interruptible(Platform* super, instant_t wakeup_time) {
  PlatformZephyr* self = (PlatformZephyr*)super;
  LF_DEBUG(PLATFORM, "Wait until interruptible " PRINTF_TIME, wakeup_time);
  interval_t sleep_duration = wakeup_time - super->get_physical_time(super);
  LF_DEBUG(PLATFORM, "Wait until interruptible for " PRINTF_TIME, wakeup_time);
  if (sleep_duration < 0) {
    return LF_OK;
  }

  int ret = k_sem_take(&self->sem, K_NSEC(sleep_duration));

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

void PlatformZephyr_notify(Platform* super) {
  PlatformZephyr* self = (PlatformZephyr*)super;
  LF_DEBUG(PLATFORM, "New async event");
  k_sem_give(&self->sem);
}

void Platform_ctor(Platform* super) {
  PlatformZephyr* self = (PlatformZephyr*)super;
  super->get_physical_time = PlatformZephyr_get_physical_time;
  super->wait_until = PlatformZephyr_wait_until;
  super->wait_for = PlatformZephyr_wait_for;
  super->wait_until_interruptible = PlatformZephyr_wait_until_interruptible;
  super->notify = PlatformZephyr_notify;
  // Initialize semaphore with initial count 0 and limit 1.
  int ret = k_sem_init(&self->sem, 0, 1);
  if (ret != 0) {
    LF_ERR(PLATFORM, "Failed to initialize semaphore");
    validate(false);
  }
}

Platform* Platform_new() { return &platform.super; }

void MutexZephyr_lock(Mutex* super) {
  MutexZephyr* self = (MutexZephyr*)super;
  validaten(k_mutex_lock(&self->mutex, K_FOREVER));
}

void MutexZephyr_unlock(Mutex* super) {
  MutexZephyr* self = (MutexZephyr*)super;
  validaten(k_mutex_unlock(&self->mutex));
}

void Mutex_ctor(Mutex* super) {
  MutexZephyr* self = (MutexZephyr*)super;
  super->lock = MutexZephyr_lock;
  super->unlock = MutexZephyr_unlock;
  validaten(k_mutex_init(&self->mutex));
}