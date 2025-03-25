#include "reactor-uc/platform/flexpret/flexpret.h"
#include "reactor-uc/error.h"

static PlatformFlexpret platform;

void Platform_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }

lf_ret_t PlatformFlexpret_initialize(Platform *super) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;
  self->async_event_occurred = false;
  self->lock = (fp_lock_t)FP_LOCK_INITIALIZER;
  return LF_OK;
}

instant_t PlatformFlexpret_get_physical_time(Platform *super) {
  (void)self;
  return (instant_t)rdtime64();
}

lf_ret_t PlatformFlexpret_wait_until_interruptible(Platform *super, instant_t wakeup_time) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;

  self->async_event_occurred = false;
  super->leave_critical_section(super);
  // For FlexPRET specifically this functionality is directly available as
  // an instruction
  // It cannot fail - if it does, there is a bug in the processor and not much
  // software can do about it
  fp_wait_until(wakeup_time);
  super->enter_critical_section(super);

  if (self->async_event_occurred) {
    return LF_SLEEP_INTERRUPTED;
  } else {
    return LF_OK;
  }
}

lf_ret_t PlatformFlexpret_wait_until(Platform *super, instant_t wakeup_time) {
  (void)self;

  // Interrupts should be disabled here so it does not matter whether we
  // use wait until or delay until, but delay until is more accurate here
  fp_delay_until(wakeup_time);
  return LF_OK;
}

lf_ret_t PlatformFlexpret_wait_for(Platform *super, interval_t wait_time) {
  (void)self;

  // Interrupts should be disabled here so it does not matter whether we
  // use wait until or delay until, but delay until is more accurate here
  fp_delay_for(wait_time);
  return LF_OK;
}

// Note: Code is directly copied from FlexPRET's reactor-c implementation;
//       beware of code duplication
void PlatformFlexpret_leave_critical_section(Platform *super) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;

  // In the special case where this function is called during an interrupt
  // subroutine (isr) it should have no effect
  if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
    return;

  self->num_nested_critical_sections--;
  if (self->num_nested_critical_sections == 0) {
    fp_interrupt_enable();
    fp_lock_release(&self->lock);
  } else if (self->num_nested_critical_sections < 0) {
    validate(false);
  }
}

// Note: Code is directly copied from FlexPRET's reactor-c implementation;
//       beware of code duplication
void PlatformFlexpret_enter_critical_section(Platform *super) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;

  // In the special case where this function is called during an interrupt
  // subroutine (isr) it should have no effect
  if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
    return;

  if (self->num_nested_critical_sections == 0) {
    fp_interrupt_disable();
    fp_lock_acquire(&self->lock);
  }
  self->num_nested_critical_sections++;
}

void PlatformFlexpret_new_async_event(Platform *super) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;
  self->async_event_occurred = true;
}

void Platform_ctor(Platform *super) {
  PlatformFlexpret *self = (PlatformFlexpret *)super;
  super->enter_critical_section = PlatformFlexpret_enter_critical_section;
  super->leave_critical_section = PlatformFlexpret_leave_critical_section;
  super->get_physical_time = PlatformFlexpret_get_physical_time;
  super->wait_until = PlatformFlexpret_wait_until;
  super->wait_for = PlatformFlexpret_wait_for;
  super->initialize = PlatformFlexpret_initialize;
  super->wait_until_interruptible_locked = PlatformFlexpret_wait_until_interruptible;
  super->new_async_event = PlatformFlexpret_new_async_event;
  self->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
