#ifndef REACTOR_UC_REACTION_H
#define REACTOR_UC_REACTION_H

#include "reactor-uc/tag.h"
#include <stdbool.h>

typedef struct Reaction Reaction;
typedef struct Reactor Reactor;
typedef struct Trigger Trigger;

typedef enum { REACTION_IDLE, REACTION_ENQUEUED, REACTION_EXECUTING } ReactionState;

struct Reaction {
  Reactor *parent;
  ReactionState state;
  void (*body)(Reaction *self);
  void (*deadline_violation_handler)(Reaction *self);
  void (*stp_violation_handler)(Reaction *self);
  interval_t deadline;
  int level; // Negative level means it is invalid.
  size_t index;
  Trigger **effects;
  size_t effects_size;
  size_t effects_registered;
  size_t (*calculate_level)(Reaction *self);
  size_t (*get_level)(Reaction *self);
};

void Reaction_ctor(Reaction *self, Reactor *parent, void (*body)(Reaction *self), Trigger **effects,
                   size_t effects_size, size_t index, void (*deadline_violation_handler)(Reaction *),
                   interval_t deadline, void (*stp_violation_handler)(Reaction *));

#endif
