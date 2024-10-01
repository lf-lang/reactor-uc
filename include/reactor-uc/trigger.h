#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/reactor_element.h"
#include "reactor-uc/trigger_value.h"
#include <stddef.h>

typedef struct Trigger Trigger;
typedef void (*Trigger_pre_trigger_hook)(Trigger *self);
struct Trigger {
  ReactorElement super;
  Reactor *parent;
  Reaction **effects;
  size_t effects_registered;
  size_t effects_size;
  Reaction **sources;
  size_t sources_size;
  size_t sources_registered;
  bool is_present;
  Trigger *next;               // For chaining together triggers, e.g. shutdown/startup triggers.
  TriggerValue *trigger_value; // Wrapper around the values/data associated with events scheduled on this trigger
  void *friend;                // Pointer to friend-class (sorry)
  void (*prepare)(Trigger *);
  void (*cleanup)(Trigger *);
  int (*schedule_at)(Trigger *, tag_t, const void *);
  int (*schedule_at_locked)(Trigger *, tag_t, const void *);
  // FIXME: Replace these two with macros. Not needed...
  void (*register_effect)(Trigger *, Reaction *);
  void (*register_source)(Trigger *, Reaction *);
} __attribute__((aligned(32)));

void Trigger_ctor(Trigger *self, ElementType type, Reactor *parent, Reaction **effects, size_t effects_size,
                  Reaction **sources, size_t sources_size, TriggerValue *trigger_value, void *friend,
                  void (*prepare)(Trigger *), void (*cleanup)(Trigger *));

#endif
