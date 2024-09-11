#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

void Reaction_ctor(Reaction *self, Reactor *parent, ReactionHandler body, int index, int (*calculate_level)(Reaction *),
                   void *typed) {
  self->body = body;
  self->parent = parent;
  self->index = index;
  self->level = -1; // -1 meaning not calculated yet
  self->calculate_level = calculate_level;
  self->typed = typed;
}
