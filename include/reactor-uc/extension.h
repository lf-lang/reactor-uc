/**
 * @file extension.h
 * @brief Statically wired runtime extensions.
 *
 * With LF_RUNTIME_EXTENSIONS=1, the application wires hooks in a configuration header
 * named by LF_EXTENSION_CONFIG_HEADER, by defining any of the `LF_EXTENSION_ON_*` macros
 * listed below. Hooks it leaves undefined, and every hook when extensions are disabled,
 * compile to nothing.
 *
 * Configuration headers must be leaf headers: core objects are incomplete here.
 * Hook adapters receive borrowed pointers and must not retain them or block.
 */
#ifndef REACTOR_UC_EXTENSION_H
#define REACTOR_UC_EXTENSION_H

#ifndef LF_RUNTIME_EXTENSIONS
#define LF_RUNTIME_EXTENSIONS 0
#endif

#include "reactor-uc/error.h"
#include "reactor-uc/event.h"
#include "reactor-uc/tag.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct Environment Environment;
typedef struct Reaction Reaction;
typedef struct Timer Timer;
typedef struct Action Action;

/** A gate is an AND of boolean conditions. An empty gate is open. LfGate is only checked when extensions enabled. */
typedef struct LfGate LfGate;
typedef struct LfGateCondition {
  const bool* value;
  struct LfGateCondition* _next;
  LfGate* _owner;
} LfGateCondition;

struct LfGate {
  LfGateCondition* _head;
};

/** Initialize a new gate. */
void LfGate_ctor(LfGate* self);
/** Initialize a new gate condition. */
void LfGateCondition_ctor(LfGateCondition* self);
/** Add a new condition to the gate. Re-adding the same condition is idempotent. */
lf_ret_t LfGate_add(LfGate* self, LfGateCondition* condition, const bool* value);
/** True when @p value is attached to @p self. */
bool LfGate_contains(const LfGate* self, const bool* value);

/** True if the gate is open. Always true with extensions disabled. */
static inline bool LfGate_is_open(const LfGate* self) {
  if (!LF_RUNTIME_EXTENSIONS) {
    return true;
  }
  for (const LfGateCondition* condition = self->_head; condition != NULL; condition = condition->_next) {
    if (!*condition->value) {
      return false;
    }
  }
  return true;
}

#if LF_RUNTIME_EXTENSIONS

/** Result of one `Action_schedule` call. */
typedef enum {
  LF_SCHEDULE_ADMITTED,       // A new event was queued.
  LF_SCHEDULE_CANCELLED,      // The previous pending event was cancelled (update policy).
  LF_SCHEDULE_REPLACED,       // The previous pending event's payload was replaced.
  LF_SCHEDULE_DROPPED_GATE,   // Scheduled event was dropped due to a closed gate.
  LF_SCHEDULE_DROPPED_POLICY, // Event was dropped due to the drop policy of the trigger.
  LF_SCHEDULE_BUFFER_FULL,    // The trigger's buffer is full and the event was dropped.
  LF_SCHEDULE_REJECTED,       // The scheduler refused the tag (past, after stop, before start).
} LfScheduleOutcome;

/** Timing violation detected before a reaction's handler replaces its body. */
typedef enum { LF_VIOLATION_DEADLINE, LF_VIOLATION_STP } LfViolationKind;

/* Gates can only be attached during setup, before the first lifecycle notification. */

/** Add a gate to a reaction, preventing it from being enqueued to the reaction queue when the gate is closed. */
lf_ret_t LfReaction_add_gate(Reaction* reaction, LfGateCondition* condition, const bool* value);
/** Add a gate to a timer, preventing it from being armed when the gate is closed. */
lf_ret_t LfTimer_add_gate(Timer* timer, LfGateCondition* condition, const bool* value);
/** Add a gate to an action, preventing it from being scheduled when the gate is closed. */
lf_ret_t LfAction_add_admission_gate(Action* action, LfGateCondition* condition, const bool* value);

/* Event control of actions, timers and delayed connections. The runtime does not track
 * which extension controls a trigger. At most one extension may control a trigger's
 * events. Operations on other trigger types return LF_INVALID_VALUE and leave the
 * event queue untouched. */

/** Maximum number of simultaneously pending events. Returns SIZE_MAX when unbounded,
 * 0 when the trigger cannot be controlled. */
size_t LfEventControl_bound(const Trigger* trigger);
/** Move every pending event of @p trigger into @p out. If more than @p capacity are
 * pending, @p capacity of them are taken, the rest stay queued and LF_VALUE_BUFFER_FULL is
 * returned. Retained events keep consuming their trigger's event budget until restored or
 * discarded. */
lf_ret_t LfEventControl_take(Trigger* trigger, Event* out, size_t capacity, size_t* count);
/** Re-insert retained events shifted by @p shift. Tags no longer after the current tag
 * move to the next microstep, while events beyond the stop tag are dropped and released. */
lf_ret_t LfEventControl_restore(Trigger* trigger, const Event* events, size_t count, interval_t shift);
/** Release retained events without delivering them. */
lf_ret_t LfEventControl_discard(Trigger* trigger, const Event* events, size_t count);
/** Remove and release every pending event of @p trigger. */
lf_ret_t LfEventControl_purge(Trigger* trigger, size_t* count);

/** Remove the timer's pending event and report its remaining delay from the current
 * tag. With nothing pending, @p remaining is NEVER and LF_EVENT_NOT_FOUND is returned. */
lf_ret_t LfTimerControl_suspend(Timer* timer, interval_t* remaining);
/** Schedule the timer's next event @p delay after the current tag, replacing any pending
 * one. If @p delay is less than or equal to zero, the event is scheduled for the next microstep. */
lf_ret_t LfTimerControl_arm(Timer* timer, interval_t delay);

/** Enqueue a reaction at the current tag. */
lf_ret_t LfExtension_enqueue_reaction(Environment* env, Reaction* reaction);

/** Owned by one environment. Lifecycle fields are only accessed by its execution context. */
typedef struct {
  void* context;
  tag_t last_completed_tag;
  bool bound;
  bool sealed;
  bool has_completed_tag;
  bool shutdown_notified;
} LfExtensionState;

void LfExtensionState_ctor(LfExtensionState* self);

/** Bind application-owned storage before execution. It is passed to every hook as its
 * first argument. Rebinding the same context is idempotent during setup. NULL is a valid
 * context, and an unbound environment also supplies NULL. */
lf_ret_t Environment_bind_extensions(Environment* self, void* context);

#ifdef LF_EXTENSION_CONFIG_HEADER
#include LF_EXTENSION_CONFIG_HEADER
#endif

#endif /* LF_RUNTIME_EXTENSIONS */

/* A hook is a function that can be overridden by an extension. If a hook is not defined,
 * it defaults to a no-op implementation. */
#ifndef LF_EXTENSION_ON_TAG_START
#define LF_EXTENSION_ON_TAG_START(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_TAG_COMPLETE
#define LF_EXTENSION_ON_TAG_COMPLETE(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_SHUTDOWN
#define LF_EXTENSION_ON_SHUTDOWN(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_NEXT_TAG
#define LF_EXTENSION_ON_NEXT_TAG(...) (FOREVER_TAG)
#endif
#ifndef LF_EXTENSION_ON_REACTION_START
#define LF_EXTENSION_ON_REACTION_START(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_REACTION_END
#define LF_EXTENSION_ON_REACTION_END(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_ACTION_SCHEDULE
#define LF_EXTENSION_ON_ACTION_SCHEDULE(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_VIOLATION
#define LF_EXTENSION_ON_VIOLATION(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_WAIT_START
#define LF_EXTENSION_ON_WAIT_START(...) ((void)0)
#endif
#ifndef LF_EXTENSION_ON_WAIT_END
#define LF_EXTENSION_ON_WAIT_END(...) ((void)0)
#endif

/* Runtime call sites. They expand only where Environment is complete. Stateless hooks
 * forward straight to the adapter. */
#define LF_EXTENSION_CONTEXT(Env) ((Env)->_extensions.context)
#define LF_EXTENSION_NEXT_TAG(Env, Tag) LF_EXTENSION_ON_NEXT_TAG(LF_EXTENSION_CONTEXT(Env), (Env), (Tag))
#define LF_EXTENSION_REACTION_START(Env, Reaction, Tag)                                                                \
  LF_EXTENSION_ON_REACTION_START(LF_EXTENSION_CONTEXT(Env), (Env), (Reaction), (Tag))
#define LF_EXTENSION_REACTION_END(Env, Reaction, Tag)                                                                  \
  LF_EXTENSION_ON_REACTION_END(LF_EXTENSION_CONTEXT(Env), (Env), (Reaction), (Tag))
#define LF_EXTENSION_ACTION_SCHEDULE(Env, Action, Offset, Tag, Outcome)                                                \
  LF_EXTENSION_ON_ACTION_SCHEDULE(LF_EXTENSION_CONTEXT(Env), (Env), (Action), (Offset), (Tag), (Outcome))
#define LF_EXTENSION_VIOLATION(Env, Reaction, Tag, Kind)                                                               \
  LF_EXTENSION_ON_VIOLATION(LF_EXTENSION_CONTEXT(Env), (Env), (Reaction), (Tag), (Kind))
#define LF_EXTENSION_WAIT_START(Env, Target) LF_EXTENSION_ON_WAIT_START(LF_EXTENSION_CONTEXT(Env), (Env), (Target))
#define LF_EXTENSION_WAIT_END(Env, Target, Result)                                                                     \
  LF_EXTENSION_ON_WAIT_END(LF_EXTENSION_CONTEXT(Env), (Env), (Target), (Result))

#if LF_RUNTIME_EXTENSIONS
static inline void LfExtension_tag_start(LfExtensionState* state, Environment* env, tag_t tag, bool shutdown) {
  (void)env;
  (void)tag;
  (void)shutdown;
  state->sealed = true;
  LF_EXTENSION_ON_TAG_START(state->context, env, tag, shutdown);
}

static inline void LfExtension_tag_complete(LfExtensionState* state, Environment* env, tag_t tag, bool shutdown) {
  (void)env;
  (void)shutdown;
  state->sealed = true;
  if (state->has_completed_tag && lf_tag_compare(tag, state->last_completed_tag) == 0) {
    return;
  }
  state->last_completed_tag = tag;
  state->has_completed_tag = true;
  LF_EXTENSION_ON_TAG_COMPLETE(state->context, env, tag, shutdown);
}

static inline void LfExtension_shutdown(LfExtensionState* state, Environment* env) {
  (void)env;
  state->sealed = true;
  if (state->shutdown_notified) {
    return;
  }
  state->shutdown_notified = true;
  LF_EXTENSION_ON_SHUTDOWN(state->context, env);
}

#define LF_EXTENSION_ENV_INIT(Env) LfExtensionState_ctor(&(Env)->_extensions)
#define LF_EXTENSION_TAG_START(Env, Tag, Shutdown) LfExtension_tag_start(&(Env)->_extensions, (Env), (Tag), (Shutdown))
#define LF_EXTENSION_TAG_COMPLETE(Env, Tag, Shutdown)                                                                  \
  LfExtension_tag_complete(&(Env)->_extensions, (Env), (Tag), (Shutdown))
#define LF_EXTENSION_SHUTDOWN(Env) LfExtension_shutdown(&(Env)->_extensions, (Env))
#else
#define LF_EXTENSION_ENV_INIT(...) ((void)0)
#define LF_EXTENSION_TAG_START(...) ((void)0)
#define LF_EXTENSION_TAG_COMPLETE(...) ((void)0)
#define LF_EXTENSION_SHUTDOWN(...) ((void)0)
#endif

#endif /* REACTOR_UC_EXTENSION_H */
