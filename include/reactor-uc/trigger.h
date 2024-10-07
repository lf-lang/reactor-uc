#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/macros.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger_value.h"
#include <stddef.h>

typedef struct Trigger Trigger;

/**
 * @brief All types of triggers, all of these can safely be casted to Trigger*
 * Note that not all of them are "true" triggers, such as normal connections
 * and output ports.
 */
typedef enum {
  TRIG_TIMER,
  TRIG_LOGICAL_ACTION,
  TRIG_PHYSICAL_ACTION,
  TRIG_INPUT,
  TRIG_OUTPUT,
  TRIG_CONN,
  TRIG_CONN_DELAYED,
  TRIG_CONN_PHYSICAL,
  TRIG_CONN_FEDERATED,
  TRIG_STARTUP,
  TRIG_SHUTDOWN
} TriggerType;

/**
 * @brief TriggerEffects wrap the fields needed to track the reactions registered
 * as effects of a certain trigger.
 */
typedef struct {
  Reaction **reactions;
  size_t size;
  size_t num_registered;
} TriggerEffects;

/**
 * @brief TriggerSources wrap the fields needed to track the reactions registered
 * as sources of a certain trigger.
 */
typedef struct {
  Reaction **reactions;
  size_t size;
  size_t num_registered;
} TriggerSources;

/**
 * @brief An abstract trigger type. Other trigger types inherit from this.
 *
 */
struct Trigger {
  TriggerType type;
  Reactor *parent;
  bool is_present;
  bool is_registered_for_cleanup; // Field used by Scheduler to avoid adding the same trigger multiple times to the
                                  // linked list of triggers registered for cleanup
  Trigger *next; // For chaining together triggers, used by Scheduler to store triggers that should be cleaned up in a
                 // linked list
  TriggerValue *trigger_value; // A pointer to a TriggerValue field in a child type, Can be NULL

  void (*prepare)(Trigger *);
  void (*cleanup)(Trigger *);
  const void *(*get)(Trigger *);
} __attribute__((aligned(MEM_ALIGNMENT))); // FIXME: This should not be necessary...

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, TriggerValue *trigger_value,
                  void (*prepare)(Trigger *), void (*cleanup)(Trigger *), const void *(*get)(Trigger *));

#endif
