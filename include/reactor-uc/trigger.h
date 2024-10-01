#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger_value.h"
#include <stddef.h>

typedef struct Trigger Trigger;

// All the types of triggers that can be casted to Trigger*
typedef enum {
  TRIG_TIMER,
  TRIG_LOGICAL_ACTION,
  TRIG_PHYSICAL_ACTION,
  TRIG_INPUT,
  TRIG_OUTPUT,
  TRIG_CONN,
  TRIG_CONN_DELAYED,
  TRIG_CONN_PHYSICAL,
  TRIG_STARTUP,
  TRIG_SHUTDOWN
} TriggerType;

// Struct wrapping a set of effects of a trigger
typedef struct {
  Reaction **reactions;
  size_t size;
  size_t num_registered;
} TriggerEffects;

// Struct wrapping a set of sources for a trigger
typedef struct {
  Reaction **reactions;
  size_t size;
  size_t num_registered;
} TriggerSources;

struct Trigger {
  TriggerType type;
  Reactor *parent;
  bool is_present;
  bool is_registered_for_cleanup;
  Trigger *next;               // For chaining together triggers, e.g. shutdown/startup triggers.
  TriggerValue *trigger_value; // Pointer to values associated with trigger.
  void (*prepare)(Trigger *);
  void (*cleanup)(Trigger *);
  const void *(*get)(Trigger *);
} __attribute__((aligned(32))); // FIXME: This should not be necessary...

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, TriggerValue *trigger_value,
                  void (*prepare)(Trigger *), void (*cleanup)(Trigger *), const void *(*get)(Trigger *));

#endif
