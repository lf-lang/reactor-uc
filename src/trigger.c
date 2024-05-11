#include "reactor-uc/environment.h"
#include "reactor-uc/trigger.h"

void Trigger_schedule_at(Trigger *self, tag_t tag) {
  self->parent->env->scheduler.event_queue.insert(&self->parent->env->scheduler.event_queue, self, tag);
}

void Trigger_ctor(Trigger *self, Reactor *parent, Reaction **effects, size_t effects_size, Reaction **sources,
                  size_t sources_size, Trigger_start_time_step start_time_step) {
  self->parent = parent;
  self->effects = effects;
  self->effects_size = effects_size;
  self->sources = sources;
  self->sources_size = sources_size;
  self->start_time_step = start_time_step;

  self->schedule_at = Trigger_schedule_at;
}
