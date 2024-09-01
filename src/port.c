
#include "reactor-uc/port.h"


void InputPort_ctor(InputPort *self, Reactor *parent, void *value_ptr, Reaction **sources, size_t sources_size) {
  self->value_ptr = value_ptr;
  Trigger_ctor(&self->super, INPUT, parent, NULL, 0, sources, sources_size, NULL);
}

void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **effects, size_t effect_size) {
  Trigger_ctor(&self->super, OUTPUT, parent, effects, effect_size, NULL, 0, NULL);
}

void InputPort_set(InputPort *self) {
  // trigger
  Trigger* trigger = &(self->connection->downstream_->super);

  trigger->schedule_at(trigger, self->connection->parent_->env->current_tag);
}


OutputPort* InputPort_get(InputPort* self) {
  return self->connection->upstream_;
}

void trigger_reactions(OutputPort* self) {
  Reactor* parent = self->super.parent;
  self->super.schedule_at(&self->super, parent->env->current_tag);
}

