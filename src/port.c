
#include "reactor-uc/port.h"

void BasePort_ctor(BasePort* self, void* typed) {
  self->inward_binding = NULL;
  self->typed = typed;
}

void InputPort_ctor(InputPort *self, Reactor *parent, void* typed, Reaction **sources, size_t sources_size) {
  self->get = InputPort_get;
  BasePort_ctor(&self->base, typed);
  (void)(parent);
  (void)(sources);
  (void)(sources_size);
  //Trigger_ctor(&self->super, INPUT, parent, NULL, 0, sources, sources_size, NULL);
}

void OutputPort_ctor(OutputPort *self, Reactor *parent, void* typed, Reaction **effects, size_t effect_size) {
  self->trigger_reactions = OutputPort_trigger_reactions;

  BasePort_ctor(&self->base, typed);
  Trigger_ctor(&self->super, OUTPUT, parent, effects, effect_size, NULL, 0, NULL);
}

void InputPort_set(InputPort *self) {
  (void)(self);
  // trigger
  //Trigger* trigger = &(self->connection->downstream_->super);

  //trigger->schedule_at(trigger, self->connection->parent_->env->current_tag);
}


BasePort* InputPort_get(InputPort* self) {
  BasePort* i = self->base.inward_binding;

  while (i->inward_binding != NULL) {
    i = i->inward_binding;
  }

  return i;
}

void InputPort_register_inward_binding(InputPort* self, BasePort* inward_binding) {
  self->base.inward_binding = inward_binding;
}

void OutputPort_trigger_reactions(OutputPort* self) {
  Reactor* parent = self->super.parent;
  self->super.schedule_at(&self->super, parent->env->current_tag);
}

