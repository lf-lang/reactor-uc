#include "reactor-uc/timer.h"
#include "reactor-uc/environment.h"

void Timer_cleanup(Trigger *self) {
  Timer *t = (Timer *)self;
  self->is_present = false;

  tag_t next_tag = lf_delay_tag(self->parent->env->current_tag, t->period);
  self->schedule_at(self, next_tag, NULL);
}

void Timer_ctor(Timer *self, Reactor *parent, instant_t offset, interval_t period, Reaction **effects,
                size_t effects_size) {
  self->offset = offset;
  self->period = period;

  Trigger_ctor((Trigger *)self, TRIG_TIMER, parent, effects, effects_size, NULL, 0, NULL, NULL, NULL, NULL,
               Timer_cleanup);

  // Schedule first
  tag_t tag = {.microstep = 0, .time = offset + self->super.parent->env->start_time};
  self->super.schedule_at(&self->super, tag, NULL);
}
