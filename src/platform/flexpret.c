#include "reactor-uc/platform/flexpret.h"

static PlatformFlexpret platform;

/**
 * One variable per hardware thread so each thread knows whether an async
 * event occurred
 */
static volatile bool _async_event_occurred[FP_THREADS]
    = THREAD_ARRAY_INITIALIZER(false);

/**
 * Keep track of the number of nested critical section enter/leave. An example
 * of correct behaviour is:
 *
 * leave (disable ints) -> leave -> enter -> leave -> enter -> enter (enable ints)
 *
 * (i.e., only disable/enable interrupts at the first and last leave/enter respectively).
 *
 * Without a check on the nested counter the same example would yield:
 *
 * leave (disable ints) -> leave -> enter (enable ints) -> leave (disable ints) ->
 * enter (enable ints) -> enter
 */
static volatile int _critical_section_num_nested[FP_THREADS]
    = THREAD_ARRAY_INITIALIZER(0);

void PlatformFlexpret_initialize(Platform *self) { (void) self; }

instant_t PlatformFlexpret_get_physical_time(Platform *self) { 
    (void) self; 
    return rdtime64();
}

WaitUntilReturn PlatformFlexpret_wait_until_interruptable(Platform *self, instant_t wakeup_time) {
    (void) self;
    
    const uint32_t hartid = read_hartid();
    _async_event_occurred[hartid] = false;
    
    self->leave_critical_section(self);
    // For FlexPRET specifically this functionality is directly available as
    // an instruction
    // It cannot fail - if it does, there is a bug in the processor and not much
    // software can do about it
    fp_wait_until(wakeup_time);
    self->enter_critical_section(self);
    
    if (_async_event_occurred[hartid]) {
        return SLEEP_INTERRUPTED;
    } else {
        return SLEEP_COMPLETED;
    }
}

WaitUntilReturn PlatformFlexpret_wait_until(Platform *self, instant_t wakeup_time) {
    (void) self;

    // Interrupts should be disabled here so it does not matter whether we
    // use wait until or delay until, but delay until is more accurate here
    fp_delay_until(wakeup_time);
    return SLEEP_COMPLETED;
}

// Note: Code is directly copied from FlexPRET's reactor-c implementation;
//       beware of code duplication
void PlatformFlexpret_leave_critical_section(Platform *self) {
    (void) self;

    // In the special case where this function is called during an interrupt
    // subroutine (isr) it should have no effect
    if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
        return;

    uint32_t hartid = read_hartid();

    if (--_critical_section_num_nested[hartid] == 0) {
        fp_interrupt_enable();
    }
    fp_assert(_critical_section_num_nested[hartid] >= 0, "Number of nested critical sections less than zero.");
}

// Note: Code is directly copied from FlexPRET's reactor-c implementation;
//       beware of code duplication
void PlatformFlexpret_enter_critical_section(Platform *self) {
    (void) self;

    // In the special case where this function is called during an interrupt
    // subroutine (isr) it should have no effect
    if ((read_csr(CSR_STATUS) & 0x04) == 0x04)
        return;

    uint32_t hartid = read_hartid();

    fp_assert(_critical_section_num_nested[hartid] >= 0, "Number of nested critical sections less than zero.");
    if (_critical_section_num_nested[hartid]++ == 0) {
        fp_interrupt_disable();
    }
}

void PlatformFlexpret_new_async_event(Platform *self) {
    (void) self;
    const uint32_t hartid = read_hartid();
    _async_event_occurred[hartid] = true;
}

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformFlexpret_enter_critical_section;
  self->leave_critical_section = PlatformFlexpret_leave_critical_section;
  self->get_physical_time = PlatformFlexpret_get_physical_time;
  self->wait_until = PlatformFlexpret_wait_until;
  self->initialize = PlatformFlexpret_initialize;
  self->wait_until_interruptable = PlatformFlexpret_wait_until_interruptable;
  self->new_async_event = PlatformFlexpret_new_async_event;
}

Platform *Platform_new(void) { return (Platform *)&platform; }
