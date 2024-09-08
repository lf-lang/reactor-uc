#include "reactor-uc/environment.h"
#include "reactor-uc/timer.h"
#include <stdio.h>


void Timer_update_value(Trigger *self) {
  tag_t next_tag = lf_delay_tag(self->parent->env->current_tag, ((Timer *)self)->period);
  printf("current: %li + %li = %li", self->parent->env->current_tag.time, ((Timer*)self)->period, next_tag.time);
  self->schedule_at(self, next_tag);
}

void Timer_ctor(Timer *self, Reactor *parent, instant_t offset, interval_t period, Reaction **effects,
                size_t effects_size) {
  self->offset = offset;
  self->period = period;

  Trigger_ctor((Trigger *)self, TIMER, parent, effects, effects_size, NULL, 0, Timer_update_value);
  Timer_update_value((Trigger *)self);
}
