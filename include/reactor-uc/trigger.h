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
#if defined(LF_RUNTIME_EXTENSIONS)
  /** Runtime-private head of statically allocated extension bindings. */
  LfExtensionBinding* _extension_bindings;
#endif
} __attribute__((aligned(MEM_ALIGNMENT)));

void Trigger_ctor(Trigger* self, TriggerType type, Reactor* parent, EventPayloadPool* payload_pool,
                  void (*prepare)(Trigger*, Event*), void (*cleanup)(Trigger*));

#if defined(LF_RUNTIME_EXTENSIONS)

/** Attach typed extension state to this trigger during initialization. */
lf_ret_t Trigger_bind_extension(Trigger* self, LfExtensionBinding* binding, const LfExtensionDescriptor* descriptor,
                                void* state);

/** Return this descriptor's state for the trigger, or NULL when it has no binding. */
void* Trigger_extension_state(const Trigger* self, const LfExtensionDescriptor* descriptor);

/** @brief Take every pending event belonging to `self` out of the event queue.
 *
 * @param out Buffer to receive the taken events.
 * @param cap Capacity of @p out.
 * @param n_out Set to the number of events taken (0 on failure).
 * @return LF_OK on success. LF_VALUE_BUFFER_FULL if more than @p cap events are
 *         pending, in which case nothing is taken.
 */
lf_ret_t Trigger_take_pending(Trigger* self, Event* out, size_t cap, size_t* n_out);

/** @brief Re-insert previously taken events, optionally shifting their tags.
 *
 * @param in_events Events previously returned by `Trigger_take_pending`.
 * @param n Number of events in @p in_events.
 * @param shift Added to each event's tag before it is rescheduled. Zero re-inserts the
 *              event at its original tag (clamped into the future if that tag is no
 *              longer ahead of the current tag).
 * @return LF_OK. Individual events that land past the stop tag are silently dropped,
 *         mirroring `schedule_at`'s own shutdown behaviour.
 */
lf_ret_t Trigger_restore_pending(Trigger* self, const Event* in_events, size_t n, interval_t shift);

/** @brief Discard previously taken events: free their payload slots and return their
 *  action's `events_scheduled` budget, without ever re-delivering them.
 *
 * @param in_events Events previously returned by `Trigger_take_pending`.
 * @param n Number of events in @p in_events.
 */
lf_ret_t Trigger_discard_pending(Trigger* self, const Event* in_events, size_t n);

/** @brief Drop every pending event of `self` from the event queue, frees their
 *  payloads.
 *
 * @param n_out Set to the number of events dropped.
 * @return LF_OK. Purging a trigger with nothing pending returns LF_OK.
 */
lf_ret_t Trigger_purge_pending(Trigger* self, size_t* n_out);

#endif /* LF_RUNTIME_EXTENSIONS */

#endif
