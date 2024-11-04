#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/event.h"
#include "reactor-uc/macros.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stddef.h>

typedef struct Trigger Trigger;

/**
 * @brief All types of triggers, all of these can safely be casted to Trigger*
 * Note that not all of them are "true" triggers, such as normal connections
 * and output ports.
 */
typedef enum {
  TRIG_TIMER,
  TRIG_ACTION,
  TRIG_INPUT,
  TRIG_OUTPUT,
  TRIG_CONN,
  TRIG_CONN_DELAYED,
  TRIG_CONN_FEDERATED_INPUT,
  TRIG_CONN_FEDERATED_OUTPUT,
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
  TriggerSources sources;
  TriggerEffects effects;
  bool is_present;
  bool is_registered_for_cleanup; // Field used by Scheduler to avoid adding the same trigger multiple times to the
                                  // linked list of triggers registered for cleanup
  Trigger *next; // For chaining together triggers, used by Scheduler to store triggers that should be cleaned up in a
                 // linked list
  EventPayloadPool *payload_pool; // A pointer to a EventPayloadPool field in a child type, Can be NULL
  void (*prepare)(Trigger *, Event *);
  void (*cleanup)(Trigger *);
} __attribute__((aligned(MEM_ALIGNMENT)));

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, EventPayloadPool *payload_pool, Reaction **sources,
                  size_t sources_size, Reaction **effects, size_t effects_size, void (*prepare)(Trigger *, Event *),
                  void (*cleanup)(Trigger *));

#endif
