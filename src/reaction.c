#include "reactor-uc/reaction.h"

void Reaction_ctor(Reaction *self, Reactor *parent, ReactionHandler body) {
  self->body = body;
  self->parent = parent;
}
