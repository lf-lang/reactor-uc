#include "reactor-uc/timer.h"
#include "reactor-uc/environment.h"

void Timer_ctor(Timer *self, Reactor *parent, instant_t offset, interval_t period, Reaction **effects,
                size_t effects_size) {
  self->offset = offset;
  self->period = period;

  Trigger_ctor((Trigger *)self, TIMER, parent, effects, effects_size, NULL, 0, NULL);

  // Schedule first
  tag_t tag = {.microstep = 0, .time = offset + self->super.parent->env->start_time};
  self->super.schedule_at(&self->super, tag, NULL);
}
