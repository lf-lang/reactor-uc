#include "reactor-uc/timer.h"
#include "reactor-uc/environment.h"

void Timer_prepare(Trigger *_self) {
  Timer *self = (Timer *)_self;
  Scheduler *sched = &_self->parent->env->scheduler;
  _self->is_present = true;
  for (size_t i = 0; i < self->effects.size; i++) {
    sched->reaction_queue.insert(&sched->reaction_queue, self->effects.reactions[i]);
  }
}

void Timer_cleanup(Trigger *_self) {
  Timer *self = (Timer *)_self;
  _self->is_present = false;
  SchedulableTrigger *sched_trigger = (SchedulableTrigger *)self;

  tag_t next_tag = lf_delay_tag(_self->parent->env->current_tag, self->period);
  sched_trigger->schedule_at(sched_trigger, next_tag, NULL);
}

void Timer_ctor(Timer *self, Reactor *parent, instant_t offset, interval_t period, Reaction **effects,
                size_t effects_size) {
  self->offset = offset;
  self->period = period;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;

  SchedulableTrigger *sched_trigger = (SchedulableTrigger *)self;
  SchedulableTrigger_ctor(sched_trigger, TRIG_TIMER, parent, NULL, Timer_prepare, Timer_cleanup);

  // Schedule first
  tag_t tag = {.microstep = 0, .time = offset + self->super.parent->env->start_time};
  sched_trigger->schedule_at(sched_trigger, tag, NULL);
}
