
#ifndef REACTOR_UC_TIMER_H
#define REACTOR_UC_TIMER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"
typedef struct Timer Timer;

struct Timer {
  Trigger super;
  instant_t offset;
  interval_t period;
  TriggerEffects effects;
  TriggerObservers observers;
#if defined(LF_RUNTIME_EXTENSIONS)
  /** AND-composed initial-arm gate. */
  LfGate gate;
  /** Storage used by LF_TIMER_SET_GATE. */
  LfGateCondition _primary_gate;
#endif
} __attribute__((aligned(MEM_ALIGNMENT)));

void Timer_ctor(Timer* self, Reactor* parent, instant_t offset, interval_t period, Reaction** effects,
                size_t effects_size, Reaction** observers, size_t observers_size);

#if defined(LF_RUNTIME_EXTENSIONS)

/** @brief Take the timer's single pending event out of the event queue and report the
 *  remaining delay until it would have fired.
 *
 * @param remaining_out Set to the remaining delay on success, or `NEVER` if the timer had
 *        no pending event. That delay is the due tag minus the current tag: the microstep
 *        is dropped, not carried, so an event due at the current time and a later microstep
 *        reports 0.
 * @return LF_OK on success, LF_EVENT_NOT_FOUND if the timer had no pending event, and
 *         LF_VALUE_BUFFER_FULL if the trigger somehow has more than the one pending event
 *         this takes room for, in which case nothing is removed from the event queue.
 */
lf_ret_t Timer_suspend(Timer* self, interval_t* remaining_out);

/** @brief Schedule the timer's next event at `lf_delay_tag(current_tag, delay)`.
 *
 * @param delay Delay from the current tag. `delay == 0` lands at the next microstep
 *        (see `lf_delay_tag`), which is what makes a zero-delay re-arm schedulable at all.
 *        A negative `delay` is silently clamped to 0 rather than rejected.
 */
void Timer_arm(Timer* self, interval_t delay);

#endif /* LF_RUNTIME_EXTENSIONS */

#endif
