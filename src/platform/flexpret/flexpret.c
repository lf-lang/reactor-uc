#include "reactor-uc/platform/flexpret/flexpret.h"
#include "reactor-uc/error.h"

static PlatformFlexpret platform;

void Platform_vprintf(const char *fmt, va_list args) { vprintf(fmt, args); }

lf_ret_t PlatformFlexpret_initialize(Platform *self) {
  PlatformFlexpret *p = (PlatformFlexpret *)self;
  p->async_event_occurred = false;
  p->in_critical_section = false;
  p->lock = (fp_lock_t)FP_LOCK_INITIALIZER;
  return LF_OK;
}

instant_t PlatformFlexpret_get_physical_time(Platform *self) {
  (void)self;
  return (instant_t)rdtime64();
}

lf_ret_t PlatformFlexpret_wait_until_interruptible(Platform *self, instant_t wakeup_time) {
  PlatformFlexpret *p = (PlatformFlexpret *)self;

  p->async_event_occurred = false;
  self->leave_critical_section(self);
  // For FlexPRET specifically this functionality is directly available as
  // an instruction
  // It cannot fail - if it does, there is a bug in the processor and not much
  // software can do about it
  fp_wait_until(wakeup_time);
  self->enter_critical_section(self);

  if (p->async_event_occurred) {
    return LF_SLEEP_INTERRUPTED;
  } else {
    return LF_OK;
  }
}

lf_ret_t PlatformFlexpret_wait_until(Platform *self, instant_t wakeup_time) {
  (void)self;

  // Interrupts should be disabled here so it does not matter whether we
  // use wait until or delay until, but delay until is more accurate here
  fp_delay_until(wakeup_time);
  return LF_OK;
}

lf_ret_t PlatformFlexpret_wait_for(Platform *self, interval_t wait_time) {
  (void)self;

  // Interrupts should be disabled here so it does not matter whether we
  // use wait until or delay until, but delay until is more accurate here
  fp_delay_for(wait_time);
  return LF_OK;
}

// Note: Code is directly copied from FlexPRET's reactor-c implementation;
//       beware of code duplication
void PlatformFlexpret_leave_critical_section(Platform *self) {
  PlatformFlexpret *p = (PlatformFlexpret *)self;

  // In the special case where this function is called during an interrupt
  // subroutine (isr) it should have no effect
  if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
    return;

  p->num_nested_critical_sections--;
  if (p->num_nested_critical_sections == 0) {
    fp_interrupt_enable();
    fp_lock_release(&p->lock);
  } else if (p->num_nested_critical_sections < 0) {
    validate(false);
  }
}

// Note: Code is directly copied from FlexPRET's reactor-c implementation;
//       beware of code duplication
void PlatformFlexpret_enter_critical_section(Platform *self) {
  PlatformFlexpret *p = (PlatformFlexpret *)self;

  // In the special case where this function is called during an interrupt
  // subroutine (isr) it should have no effect
  if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
    return;

  if (p->num_nested_critical_sections == 0) {
    fp_interrupt_disable();
    fp_lock_acquire(&p->lock);
  }
  p->num_nested_critical_sections++;
}

void PlatformFlexpret_new_async_event(Platform *self) {
  PlatformFlexpret *p = (PlatformFlexpret *)self;
  p->async_event_occurred = true;
}

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformFlexpret_enter_critical_section;
  self->leave_critical_section = PlatformFlexpret_leave_critical_section;
  self->get_physical_time = PlatformFlexpret_get_physical_time;
  self->wait_until = PlatformFlexpret_wait_until;
  self->wait_for = PlatformFlexpret_wait_for;
  self->initialize = PlatformFlexpret_initialize;
  self->wait_until_interruptible = PlatformFlexpret_wait_until_interruptible;
  self->new_async_event = PlatformFlexpret_new_async_event;
  ((PlatformFlexpret *)self)->num_nested_critical_sections = 0;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
