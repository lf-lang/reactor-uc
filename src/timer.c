#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/timer.h"

#include <assert.h>

void Timer_prepare(Trigger *_self) {
  LF_DEBUG(TRIG, "Preparing timer %p", _self);
  Timer *self = (Timer *)_self;
  Scheduler *sched = &_self->parent->env->scheduler;
  _self->is_present = true;
  sched->register_for_cleanup(sched, _self);
  LF_DEBUG(TRIG, "Triggering %d reactions", self->effects.size);
  for (size_t i = 0; i < self->effects.size; i++) {
    validaten(sched->reaction_queue.insert(&sched->reaction_queue, self->effects.reactions[i]));
  }
}

void Timer_cleanup(Trigger *_self) {
  Timer *self = (Timer *)_self;
  Environment *env = _self->parent->env;
  Scheduler *sched = &env->scheduler;
  _self->is_present = false;

  // Schedule next event unless it is a single-shot timer.
  if (self->period > NEVER) {
    tag_t next_tag = lf_delay_tag(sched->current_tag, self->period);
    sched->schedule_at(sched, _self, next_tag);
  }
}

void Timer_ctor(Timer *self, Reactor *parent, instant_t offset, interval_t period, Reaction **effects,
                size_t effects_size) {
  self->offset = offset;
  self->period = period;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;

  Trigger_ctor(&self->super, TRIG_TIMER, parent, NULL, Timer_prepare, Timer_cleanup, NULL);
}
