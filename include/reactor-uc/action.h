#ifndef REACTOR_UC_ACTION_H
#define REACTOR_UC_ACTION_H

#include "reactor-uc/error.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"

typedef struct Action Action;
typedef struct LogicalAction LogicalAction;
typedef struct PhysicalAction PhysicalAction;

struct Action {
  Trigger super;          // Inherit from Trigger
  interval_t min_offset;  // The minimum offset from the current time that an event can be scheduled on this action.
  interval_t min_spacing; // The minimum spacing between two consecutive events on this action.
  tag_t previous_event;   // Used to enforce min_spacing
  TriggerEffects effects; // The reactions triggered by this Action.
  TriggerSources sources; // The reactions that can write to this Action.
  TriggerDataQueue trigger_data_queue; // FIFO storage of the data associated with the events scheduled on this action.
  /**
   * @brief  Schedule an event on this action.
   */
  lf_ret_t (*schedule)(Action *self, interval_t offset, const void *value);
};

/**
 * @brief Construct an Action object. An Action is an abstract type that can be either a LogicalAction or a
 * PhysicalAction.
 *
 * @param self Pointer to an allocated Action struct.
 * @param type The type of the action. Either logical or physical.
 * @param min_offset The minimum offset from the current time that an event can be scheduled on this action.
 * @param min_spacing The minimum spacing between two consecutive events on this action.
 * @param parent The parent Reactor of this Action.
 * @param sources Pointer to an array of Reaction pointers that can be used to store the sources of this action.
 * @param sources_size The size of the sources array.
 * @param effects Pointer to an array of Reaction pointers that can be used to store the effects of this action.
 * @param effects_size The size of the effects array.
 * @param value_buf A pointer to a buffer where the data of this action is stored. This should be a field in the
 * user-defined
 * @param value_size The size of each data element that can be scheduled on this action.
 * @param value_capacity The lenght of the buffer where the data of this action is stored
 * @param schedule The function that is used to schedule an event on this action.
 */
void Action_ctor(Action *self, TriggerType type, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                 Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size, void *value_buf,
                 size_t value_size, size_t value_capacity, lf_ret_t (*schedule)(Action *, interval_t, const void *));

struct LogicalAction {
  Action super;
};

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                        Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                        void *value_buf, size_t value_size, size_t value_capacity);

struct PhysicalAction {
  Action super;
};

void PhysicalAction_ctor(PhysicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                         Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                         void *value_buf, size_t value_size, size_t value_capacity);
#endif
