#ifndef REACTOR_UC_ACTION_H
#define REACTOR_UC_ACTION_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

typedef struct Action Action;
typedef struct LogicalAction LogicalAction;
typedef struct PhysicalAction PhysicalAction;

struct Action {
  Trigger super;
  interval_t min_offset;
  interval_t min_spacing;
  tag_t previous_event;
  void *value_ptr;
  void *next_value_ptr;
  size_t value_size;
  /**
   * @brief This function schedules an event for the logical action. The value
   * associated with the event must already have been placed in
   *
   */
  void (*schedule)(Action *self, interval_t offset);
};

void Action_ctor(Action *self, interval_t min_offset, interval_t min_spacing, Reactor *parent, Reaction **sources,
                 size_t sources_size, Reaction **effects, size_t effects_size, void *value_ptr, void *next_value_ptr,
                 size_t value_size, void (*schedule)(Action *, interval_t));

struct LogicalAction {
  Action super;
};

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                        Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                        void *value_ptr, void *next_value_ptr, size_t value_size);

struct PhysicalAction {
  Action super;
};

void PhysicalAction_ctor(PhysicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                         Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                         void *value_ptr, void *next_value_ptr, size_t value_size);
#endif
