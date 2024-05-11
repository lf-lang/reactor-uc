#ifndef REACTOR_UC_ACTION_H
#define REACTOR_UC_ACTION_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include <stdbool.h>

typedef struct Action_int Action_int;

struct Action_int {
  Trigger super;
  bool is_present;
  int value;
  int next_value;
};

void Action_int_ctor(Action_int *self, Reactor *parent, Reaction **effects, size_t effects_size, Reaction **sources,
                     size_t sources_size);
void Action_int_start(Trigger *self);

#endif
