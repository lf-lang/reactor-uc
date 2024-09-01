#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

void Reaction_ctor(Reaction *self, Reactor *parent, ReactionHandler body) {
  self->body = body;
  self->parent = parent;
}
