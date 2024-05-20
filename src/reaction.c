#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

void Reaction_register_effect(Reaction *self, Trigger *effect) {
  assert(self->effects_registered < self->effects_size);
  self->effects[self->effects_registered++] = effect;
}

void Reaction_ctor(Reaction *self, Reactor *parent, ReactionHandler body, Trigger **effects, size_t effects_size) {
  self->body = body;
  self->parent = parent;
  self->effects = effects;
  self->effects_size = effects_size;
  self->effects_registered = 0;
  self->register_effect = Reaction_register_effect;
}
