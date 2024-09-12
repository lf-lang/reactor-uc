#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stddef.h>

typedef struct Trigger Trigger;

typedef enum { ACTION, TIMER, BUILTIN, INPUT, OUTPUT } TriggerType;
typedef enum { SCHEDULED, NOTSCHEDULED } TriggerReply;

typedef void (*Trigger_update_value)(Trigger *self);

struct Trigger {
  // methods
  TriggerReply (*schedule_at)(Trigger *, tag_t);
  void (*register_effect)(Trigger *, Reaction *);
  void (*register_source)(Trigger *, Reaction *);

  // member variables
  Reactor *parent;

  // reactions that will be activated by this trigger
  Reaction **effects;
  // reactions triggered that can activate this trigger
  Reaction **sources;

  size_t effects_registered;
  size_t effects_size;
  size_t sources_size;
  size_t sources_registered;
  Trigger_update_value update_value;

  TriggerType type;
  bool is_present;
}; //__attribute__((aligned(32)));

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
                  Reaction **sources, size_t sources_size, Trigger_update_value update_value_func);

#endif
