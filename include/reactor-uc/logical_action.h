#ifndef REACTOR_UC_LOGICAL_ACTION_H
#define REACTOR_UC_LOGICAL_ACTION_H

#include "reactor-uc/logical_action.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

typedef struct {
  Trigger super;
  interval_t min_offset;
  void *value_ptr;
  void *next_value_ptr;
  size_t value_size;
} LogicalAction;

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, Reactor *parent, Reaction **sources,
                        size_t sources_size, Reaction **effects, size_t effects_size, void *value_ptr,
                        void *next_value_ptr, size_t value_size);

#endif
