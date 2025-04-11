#include "reactor-uc/platform/flexpret/flexpret.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/error.h"

static PlatformFlexpret platform;

void Platform_vprintf(const char *fmt, va_list args) {
  vprintf(fmt, args);
}

instant_t PlatformFlexpret_get_physical_time(Platform *super) {
  (void)super;
  return (instant_t)rdtime64();
}

lf_ret_t PlatformFlexpret_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;

  if (self->async_event_occurred) {
    self->async_event_occurred = false;
    return LF_SLEEP_INTERRUPTED;
  }

  fp_wait_until(wakeup_time);

  if (self->async_event_occurred) {
    self->async_event_occurred = false;
    return LF_SLEEP_INTERRUPTED;
  } else {
    return LF_OK;
  }
}

lf_ret_t PlatformFlexpret_wait_until(Platform *super, instant_t wakeup_time) {
  (void)super;

  fp_delay_until(wakeup_time);
  return LF_OK;
}

lf_ret_t PlatformFlexpret_wait_for(Platform *super, interval_t wait_time) {
  (void)super;

  // Interrupts should be disabled here so it does not matter whether we
  // use wait until or delay until, but delay until is more accurate here
  fp_delay_for(wait_time);
  return LF_OK;
}

void PlatformFlexpret_notify(Platform *super) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;
  self->async_event_occurred = true;
}

void Platform_ctor(Platform *super) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;
  super->get_physical_time = PlatformFlexpret_get_physical_time;
  super->wait_until = PlatformFlexpret_wait_until;
  super->wait_for = PlatformFlexpret_wait_for;
  super->wait_until_interruptible = PlatformFlexpret_wait_until_interruptible;
  super->notify = PlatformFlexpret_notify;
  self->num_nested_critical_sections = 0;
  self->async_event_occurred = false;
  self->mutex = (fp_lock_t)FP_LOCK_INITIALIZER;
}

Platform *Platform_new() {
  return &platform.super;
}

void MutexFlexpret_unlock(Mutex *super) {
  MutexFlexpret *self = (MutexFlexpret *)super;
  PlatformFlexpret *platform = (PlatformFlexpret *)_lf_environment->platform;
  // In the special case where this function is called during an interrupt
  // subroutine (isr) it should have no effect
  if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
    return;

  platform->num_nested_critical_sections--;
  if (platform->num_nested_critical_sections == 0) {
    fp_interrupt_enable();
    fp_lock_release(&self->mutex);
  } else if (platform->num_nested_critical_sections < 0) {
    validate(false);
  }
}

void MutexFlexpret_lock(Mutex *super) {
  MutexFlexpret *self = (MutexFlexpret *)super;
  PlatformFlexpret *platform = (PlatformFlexpret *)_lf_environment->platform;
  // In the special case where this function is called during an interrupt
  // subroutine (isr) it should have no effect
  if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
    return;

  if (platform->num_nested_critical_sections == 0) {
    fp_interrupt_disable();
    fp_lock_acquire(&self->mutex);
  }
  platform->num_nested_critical_sections++;
}

void Mutex_ctor(Mutex *super) {
  MutexFlexpret *self = (MutexFlexpret *)super;
  super->lock = MutexFlexpret_lock;
  super->unlock = MutexFlexpret_unlock;
  self->mutex = (fp_lock_t)FP_LOCK_INITIALIZER;
}