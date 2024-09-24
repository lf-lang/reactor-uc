#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger_value.h"
#include <stddef.h>

#define TRIGGER_CAST(var) ((Trigger *)(var))

typedef struct Trigger Trigger;

typedef enum { ACTION, TIMER, STARTUP, SHUTDOWN, INPUT, OUTPUT } TriggerType;

typedef void (*Trigger_pre_trigger_hook)(Trigger *self);
struct Trigger {
  TriggerType type;
  Reactor *parent;
  Reaction **effects;
  size_t effects_registered;
  size_t effects_size;
  Reaction **sources;
  size_t sources_size;
  size_t sources_registered;
  bool is_present;
  bool is_scheduled;           // Isnt needed.
  Trigger *next;               // For chaining together triggers, e.g. shutdown/startup triggers.
  TriggerValue *trigger_value; // Wrapper around the values/data associated with events scheduled on this trigger
  void (*prepare)(Trigger *);
  void (*schedule_now)(Trigger *, const void *);
  int (*schedule_at)(Trigger *, tag_t, const void *);
  int (*schedule_at_locked)(Trigger *, tag_t, const void *);
  void (*register_effect)(Trigger *, Reaction *);
  void (*register_source)(Trigger *, Reaction *);
};

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
                  Reaction **sources, size_t sources_size, TriggerValue *trigger_value);

#endif
