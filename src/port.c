#include "reactor-uc/environment.h"
#include "reactor-uc/port.h"
#include "reactor-uc/scheduler.h"

void InputPort_ctor(InputPort *self, Reactor *parent, Reaction **effects, size_t effects_size, void **value_ptr_ptr) {
  Trigger_ctor(&self->super, INPUT, parent, effects, effects_size, NULL, 0, NULL);
  self->value_ptr_ptr = value_ptr_ptr;
}

void OutputPort_trigger_downstreams(OutputPort *self) {
  Scheduler *sched = &self->super.parent->env->scheduler;
  for (int i = 0; i < self->conn->downstreams_size; i++) {
    // TODO: Move this to a function `self->trigger_reactions(self, event.trigger);
    for (int j = 0; j < self->conn->downstreams[i]->super.effects_size; j++) {
      sched->reaction_queue.insert(&sched->reaction_queue, self->conn->downstreams[i]->super.effects[j]);
    }
  }
}

void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size, void *value_ptr) {
  Trigger_ctor(&self->super, OUTPUT, parent, NULL, 0, sources, sources_size, NULL);
  self->value_ptr = value_ptr;
  self->trigger_downstreams = OutputPort_trigger_downstreams;
}
