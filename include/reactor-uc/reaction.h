#ifndef REACTOR_UC_REACTION_H
#define REACTOR_UC_REACTION_H

#include "reactor-uc/tag.h"
#include <stdbool.h>

typedef struct Reaction Reaction;
typedef struct Reactor Reactor;
typedef struct Trigger Trigger;

typedef int (*ReactionHandler)(Reaction *);

struct Reaction {
  Reactor *parent;
  ReactionHandler body;
  int level;
  Trigger **effects;
  size_t effects_size;
  size_t effects_registered;
  void (*register_effect)(Reaction *self, Trigger *effect);
};

void Reaction_ctor(Reaction *self, Reactor *parent, ReactionHandler body, Trigger **effects, size_t effects_size);

#endif
