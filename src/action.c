#include "reactor-uc/action.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"

void Action_int_start(Trigger *_self) {
  Action_int *self = (Action_int *)_self;
  self->value = self->next_value;
}

void Action_int_ctor(Action_int *self, Reactor *parent, Reaction **effects, size_t effects_size, Reaction **sources,
                     size_t sources_size) {
  Trigger_ctor(&self->super, parent, effects, effects_size, sources, sources_size, &Action_int_start);
}
