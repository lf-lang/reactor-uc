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

  // function that can calculate the level for this reaction
  int (*calculate_level)(Reaction *);

  // index of the reaction
  int index;

  // the level will be calculated by the runtime at startup
  int level;

  void *typed;
};

void Reaction_ctor(Reaction *self, Reactor *parent, ReactionHandler body, int index, int (*calculate_level)(Reaction *),
                   void *typed);

#endif
