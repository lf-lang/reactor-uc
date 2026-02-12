#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/event.h"
#include "reactor-uc/consts.h"
#include "reactor-uc/reaction.h"
#include <stddef.h>

typedef struct Trigger Trigger;

/**
 * @brief All types of triggers, all of these can safely be casted to Trigger*
 * Note that not all of them are "true" triggers, such as normal connections
 * and output ports.
 */
typedef enum {
  TRIG_TIMER = 0,
  TRIG_ACTION = 1,
  TRIG_INPUT = 2,
  TRIG_OUTPUT = 3,
  TRIG_CONN = 4,
  TRIG_CONN_DELAYED = 5,
  TRIG_CONN_FEDERATED_INPUT = 6,
  TRIG_CONN_FEDERATED_OUTPUT = 7,
  TRIG_STARTUP = 8,
  TRIG_SHUTDOWN = 9
} TriggerType;

/**
 * @brief TriggerEffects wrap the fields needed to track the reactions registered
 * as effects of a certain trigger.
 */
typedef struct {
  Reaction** reactions;
  size_t size;
  size_t num_registered;
} TriggerEffects;

/**
 * @brief TriggerSources wrap the fields needed to track the reactions registered
 * as sources of a certain trigger.
 */
typedef struct {
  Reaction** reactions;
  size_t size;
  size_t num_registered;
} TriggerSources;

/**
 * @brief TriggerObserver wrap the fields needed to track the reactions registered
 * as observers for a certain trigger.
 */
typedef struct {
  Reaction** reactions;
  size_t size;
  size_t num_registered;
} TriggerObservers;

/**
 * @brief An abstract trigger type. Other trigger types inherit from this.
 *
 */
struct Trigger {
  TriggerType type;
  Reactor* parent;
  bool is_present;
  bool is_registered_for_cleanup; // Field used by Scheduler to avoid adding the same trigger multiple times to the
                                  // linked list of triggers registered for cleanup
  Trigger* next; // For chaining together triggers, used by Scheduler to store triggers that should be cleaned up in a
                 // linked list
  EventPayloadPool* payload_pool; // A pointer to a EventPayloadPool field in a child type, Can be NULL
  void (*prepare)(Trigger*, Event*);
  void (*cleanup)(Trigger*);
} __attribute__((aligned(MEM_ALIGNMENT)));

void Trigger_ctor(Trigger* self, TriggerType type, Reactor* parent, EventPayloadPool* payload_pool,
                  void (*prepare)(Trigger*, Event*), void (*cleanup)(Trigger*));

#endif
