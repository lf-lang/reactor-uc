#include "reactor-uc/timer.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"

void Timer_prepare(Trigger* _self, Event* event) {
  (void)event;
  LF_DEBUG(TRIG, "Preparing timer %p", _self);
  Timer* self = (Timer*)_self;
  Scheduler* sched = _self->parent->env->scheduler;
  _self->is_present = true;
  sched->register_for_cleanup(sched, _self);
  LF_DEBUG(TRIG, "Triggering %d reactions", self->effects.size);
  for (size_t i = 0; i < self->effects.size; i++) {
    Reaction* reaction = self->effects.reactions[i];
    if (!Reaction_may_enqueue(reaction)) {
      continue;
    }
    validaten(sched->add_to_reaction_queue(sched, reaction));
  }
}

void Timer_cleanup(Trigger* _self) {
  Timer* self = (Timer*)_self;
  Environment* env = _self->parent->env;
  Scheduler* sched = env->scheduler;
  _self->is_present = false;

  // Schedule next event unless it is a single-shot timer.
  if (self->period > NEVER) {
    tag_t next_tag = lf_delay_tag(sched->current_tag(sched), self->period);
    Event event = EVENT_INIT(next_tag, _self, NULL);
    sched->schedule_at(sched, &event);
  }
}

void Timer_ctor(Timer* self, Reactor* parent, instant_t offset, interval_t period, Reaction** effects,
                size_t effects_size, Reaction** observers, size_t observers_size) {

  self->offset = offset;
  self->period = period;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;
  self->observers.reactions = observers;
  self->observers.size = observers_size;
  self->observers.num_registered = 0;

#if defined(LF_RUNTIME_EXTENSIONS)
  LfGate_ctor(&self->gate);
  LfGateCondition_ctor(&self->_primary_gate);
#endif

  Trigger_ctor(&self->super, TRIG_TIMER, parent, NULL, Timer_prepare, Timer_cleanup);
}

#if defined(LF_RUNTIME_EXTENSIONS)

lf_ret_t Timer_suspend(Timer* self, interval_t* remaining_out) {
  validate(remaining_out);
  Trigger* trig = &self->super;
  Scheduler* sched = trig->parent->env->scheduler;
  Event taken[1];
  size_t n_taken = 0;

  lf_ret_t ret = Trigger_take_pending(trig, taken, 1, &n_taken);
  if (ret != LF_OK) {
    *remaining_out = NEVER;
    return ret;
  }
  if (n_taken == 0) {
    // Single-shot timer that already fired, or one that was never armed.
    *remaining_out = NEVER;
    return LF_EVENT_NOT_FOUND;
  }

  *remaining_out = taken[0].super.tag.time - sched->current_tag(sched).time;
  // A timer carries no payload (Timer_ctor passes a NULL pool) and is not a TRIG_ACTION,
  // so Trigger_discard_pending is a no-op.
  return Trigger_discard_pending(trig, taken, n_taken);
}

void Timer_arm(Timer* self, interval_t delay) {
  Trigger* trig = &self->super;
  Scheduler* sched = trig->parent->env->scheduler;

  tag_t tag = lf_delay_tag(sched->current_tag(sched), delay < 0 ? 0 : delay);
  Event event = EVENT_INIT(tag, trig, NULL);
  lf_ret_t ret = sched->schedule_at(sched, &event);
  if (ret != LF_OK) {
    // LF_AFTER_STOP_TAG during shutdown is expected.
    LF_DEBUG(TRIG, "Arming timer %p at delay " PRINTF_TIME " returned %d", (void*)self, delay, ret);
  }
}

#endif /* LF_RUNTIME_EXTENSIONS */
