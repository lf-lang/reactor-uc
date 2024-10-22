#ifndef REACTOR_UC_ACTION_H
#define REACTOR_UC_ACTION_H

#include "reactor-uc/error.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"

typedef struct Action Action;
typedef struct LogicalAction LogicalAction;
typedef struct PhysicalAction PhysicalAction;

typedef enum { LOGICAL_ACTION, PHYSICAL_ACTION } ActionType;

struct Action {
  Trigger super; // Inherit from Trigger
  ActionType type;
  interval_t min_offset; // The minimum offset from the current time that an event can be scheduled on this action.
  void *value_ptr;
  TriggerEffects effects;        // The reactions triggered by this Action.
  TriggerSources sources;        // The reactions that can write to this Action.
  EventPayloadPool payload_pool; // Pool of memory for the data associated with the events scheduled on this action.
  /**
   * @brief  Schedule an event on this action.
   */
  lf_ret_t (*schedule)(Action *self, interval_t offset, const void *value);
};

void Action_ctor(Action *self, ActionType type, interval_t min_offset, Reactor *parent, Reaction **sources,
                 size_t sources_size, Reaction **effects, size_t effects_size, void *value_ptr, size_t value_size,
                 void *payload_buf, bool *payload_used_buf, size_t payload_buf_capacity);

#endif
