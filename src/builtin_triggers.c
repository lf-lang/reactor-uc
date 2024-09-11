#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"

void Startup_start(Trigger *self) {
  if (!self->parent->env->running) {
    self->schedule_at(self, self->parent->env->current_tag);
  }
}
void Startup_ctor(Startup *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, BUILTIN, parent, effects, effects_size, NULL, 0, Startup_start);
}

void LogicalAction_ctor(LogicalAction *self, Reactor *parent, Reaction **effects, size_t effects_size,
                        Reaction **sources, size_t source_size) {
  Trigger_ctor((Trigger *)self, BUILTIN, parent, effects, effects_size, sources, source_size, NULL);
}

void LogicalAction_schedule(LogicalAction *self, interval_t in) {
  tag_t next_tag = lf_delay_tag(self->super.parent->env->current_tag, in);
  self->super.schedule_at(&self->super, next_tag);
}
