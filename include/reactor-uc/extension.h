/**
 * @file extension.h
 * @brief Versioned, allocation-free runtime extension contracts.
 *
 * Extensions are statically linked components that observe documented scheduler lifecycle
 * points and optionally attach typed state to triggers. The runtime owns registration and
 * dispatch; an extension owns the storage for its descriptor, instance, bindings and gates.
 */
#ifndef REACTOR_UC_EXTENSION_H
#define REACTOR_UC_EXTENSION_H

#if defined(LF_RUNTIME_EXTENSIONS)

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Environment Environment;
typedef struct Trigger Trigger;

/** Increment whenever an extension-visible structure or callback contract changes. */
#define LF_RUNTIME_EXTENSION_API_VERSION UINT16_C(1)

typedef enum {
  LF_EXTENSION_CAP_TAG_COMPLETE = UINT32_C(1) << 0,
  LF_EXTENSION_CAP_SHUTDOWN = UINT32_C(1) << 1,
  LF_EXTENSION_CAP_TRIGGER_BINDING = UINT32_C(1) << 2,
  LF_EXTENSION_CAP_COMPOSABLE_GATES = UINT32_C(1) << 3,
  LF_EXTENSION_CAP_TAG_START = UINT32_C(1) << 4,
  LF_EXTENSION_CAP_MICROSTEP_REQUEST = UINT32_C(1) << 5
} LfExtensionCapability;

#define LF_RUNTIME_EXTENSION_CAPABILITIES                                                                             \
  (LF_EXTENSION_CAP_TAG_COMPLETE | LF_EXTENSION_CAP_SHUTDOWN | LF_EXTENSION_CAP_TRIGGER_BINDING |                    \
   LF_EXTENSION_CAP_COMPOSABLE_GATES | LF_EXTENSION_CAP_TAG_START | LF_EXTENSION_CAP_MICROSTEP_REQUEST)

/** Context supplied after a scheduler tag has been completely cleaned up. */
typedef struct {
  Environment* environment;
  tag_t tag;
  // True for the final cleanup pass performed by Scheduler_do_shutdown. 
  bool is_shutdown;
} LfExtensionTagContext;

/**
 * Immutable extension type metadata and callbacks.
 *
 * A descriptor normally has static const storage and may be shared by any number of
 * instances. `struct_size` permits a future runtime to accept a backward-compatible prefix. Version 1 requires it to be at least sizeof(LfExtensionDescriptor).
 */
typedef struct LfExtensionDescriptor {
  uint16_t api_version;
  uint16_t struct_size;
  uint32_t required_capabilities;
  const char* name;
  /** Called once per tag, after the tag is committed and the reaction queue reset, 
   *  and before any of the tag's events are prepared. This is the only point at which 
   *  an extension may enqueue a reaction at an arbitrary level: `ReactionQueue_insert`
   *  asserts `curr_level <= reaction->level`, and `Scheduler_prepare_timestep` has 
   *  just reset `curr_level` to -1. Reactions enqueued here precede everything the 
   *  tag's own triggers would enqueue, and `ReactionQueue_insert` de-duplicates, 
   *  so a reaction a trigger would also enqueue keeps the position this callback gave 
   *  it.
   *
   *  Not idempotent per tag, unlike on_tag_complete: `Scheduler_do_shutdown` 
   *  dispatches it for the shutdown tag unconditionally, so a shutdown tag that 
   *  reuses the last ordinary tag is notified twice. An extension acting here on 
   *  state its own on_tag_complete wrote must test `context->is_shutdown`. 
   */
  void (*on_tag_start)(void* state, const LfExtensionTagContext* context);
  void (*on_tag_complete)(void* state, const LfExtensionTagContext* context);
  void (*on_shutdown)(void* state, Environment* environment);

} LfExtensionDescriptor;

/**
 * One registered extension instance. Its storage must outlive its Environment.
 */
typedef struct LfExtension {
  const LfExtensionDescriptor* descriptor;
  void* state;
  Environment* _owner;
  struct LfExtension* _next;
} LfExtension;

#define LF_EXTENSION_INSTANCE_INIT(Descriptor, State)                                                                \
  {.descriptor = (Descriptor), .state = (State), ._owner = NULL, ._next = NULL}

/** Initialize an unowned instance once, before registration. Never reinitialize a registered instance. */
void LfExtension_ctor(LfExtension* self, const LfExtensionDescriptor* descriptor, void* state);

/**
 * Register an extension during Environment initialization.
 *
 * Registration is idempotent for the same instance and Environment. Reusing an 
 * instance in another Environment, registering after the first tag starts 
 * dispatching, or requesting an unsupported API/capability returns 
 * LF_INVALID_VALUE without modifying either registry.
 * Callback order is registration order.
 */
lf_ret_t Environment_register_extension(Environment* self, LfExtension* extension);

// Runtime scheduler join point. Extension clients do not normally call this directly. 
void Environment_notify_tag_start(Environment* self, tag_t tag, bool is_shutdown);
void Environment_notify_tag_complete(Environment* self, tag_t tag, bool is_shutdown);

// Runtime shutdown join point. Idempotent if called more than once.
void Environment_notify_shutdown(Environment* self);


/** One boolean condition that can be attached to a gate. A gate is open when 
 *  every condition is true. */
typedef struct LfGateCondition {
  const bool* value;
  struct LfGateCondition* _next;
  const struct LfGate* _owner;
} LfGateCondition;

typedef struct LfGate {
  LfGateCondition* _head;
} LfGate;

void LfGate_ctor(LfGate* self);
/** Initialize an unattached condition once. Never reinitialize a condition linked into a gate. */
void LfGateCondition_ctor(LfGateCondition* self);

/** Add a condition to a gate. Re-adding the same condition to that gate is idempotent. */
lf_ret_t LfGate_add(LfGate* self, LfGateCondition* condition, const bool* value);

/** True when every condition is true; an empty gate is open. */
static inline bool LfGate_is_open(const LfGate* self) {
  for (const LfGateCondition* condition = self->_head; condition != NULL; condition = condition->_next) {
    if (!*condition->value) {
      return false;
    }
  }
  return true;
}

/** True when `value` is one of this gate's conditions. Useful for wiring validation. */
bool LfGate_contains(const LfGate* self, const bool* value);

/**
 * Typed extension state attached directly to a Trigger.
 *
 * A Trigger may have at most one binding for a descriptor. This makes ownership lookup
 * constant in the number of registered extension instances and detects duplicate ownership
 * during initialization.
 */
typedef struct LfExtensionBinding {
  const LfExtensionDescriptor* descriptor;
  void* state;
  Trigger* _owner;
  struct LfExtensionBinding* _next;
} LfExtensionBinding;

/** Initialize an unattached binding once. Never reinitialize a binding linked into a trigger. */
void LfExtensionBinding_ctor(LfExtensionBinding* self);

#endif /* LF_RUNTIME_EXTENSIONS */
#endif /* REACTOR_UC_EXTENSION_H */
