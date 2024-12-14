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
  Trigger super;
  ActionType type;
  interval_t min_offset;     // The minimum offset from the current time that an event can be scheduled on this action.
  interval_t min_spacing;    // The minimum spacing between two events scheduled on this action.
  instant_t last_event_time; // Logical time of most recent event scheduled on this action.
  void *value_ptr;           // Pointer to the value associated with this action at the current logical tag.
  TriggerEffects effects;        // The reactions triggered by this Action.
  TriggerSources sources;        // The reactions that can write to this Action.
  TriggerObservers observers;    // The reactions that can observe this Action.
  EventPayloadPool payload_pool; // Pool of memory for the data associated with the events scheduled on this action.
  size_t max_pending_events;  // The maximum number of events that can be scheduled on this action.
  size_t events_scheduled;  // The number of events currently scheduled on this action.

  /**
   * @brief  Schedule an event on this action.
   * 
   * @param self The action to schedule the event on.
   * @param offset The tag of the scheduled event will be the current tag plus the min_offset plus this offset.
   * @param value A pointer to the value which should be scheduled with the event.
   */
  lf_ret_t (*schedule)(Action *self, interval_t offset, const void *value);
};

void Action_ctor(Action *self, ActionType type, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                 Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size, Reaction **observers,
                 size_t observers_size, void *value_ptr, size_t value_size, void *payload_buf, bool *payload_used_buf,
                 size_t event_bound);

#endif
