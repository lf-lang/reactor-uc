#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stddef.h>

typedef struct Trigger Trigger;

typedef enum { ACTION, TIMER, STARTUP, SHUTDOWN, INPUT, OUTPUT } TriggerType;

typedef void (*Trigger_update_value)(Trigger *self);
struct Trigger {
  TriggerType type;
  Reactor *parent;
  Reaction **effects;
  size_t effects_registered;
  size_t effects_size;
  Reaction **sources;
  size_t sources_size;
  size_t sources_registered;
  Trigger_update_value update_value;
  bool is_present;
  Trigger *next; // For chaining together triggers, e.g. shutdown/startup triggers.
  void (*schedule_at)(Trigger *, tag_t);
  void (*register_effect)(Trigger *, Reaction *);
  void (*register_source)(Trigger *, Reaction *);
};

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
                  Reaction **sources, size_t sources_size, Trigger_update_value update_value_func);

#endif
