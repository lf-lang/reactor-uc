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
  tag_t previous_event; // Used to enforce min_spacing
  TriggerEffects effects;
  TriggerSources sources;
  TriggerValue trigger_value; // This is where data associated with schedueled events are stored
  void (*schedule)(Action *self, interval_t offset, const void *value);
};

void Action_ctor(Action *self, TriggerType type, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                 Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size, void *value_buf,
                 size_t value_size, size_t value_capacity, void (*schedule)(Action *, interval_t, const void *));

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
