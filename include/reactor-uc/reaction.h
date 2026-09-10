#ifndef REACTOR_UC_REACTION_H
#define REACTOR_UC_REACTION_H

#include "reactor-uc/tag.h"
#if defined(LF_RUNTIME_EXTENSIONS)
#include "reactor-uc/extension.h"
#endif
#include <stdbool.h>

typedef struct Reaction Reaction;

/** @brief `Reaction.level` before it has been computed. */
#define LF_LEVEL_UNSET (-1)
/** @brief `Reaction.level` while it is BEING computed. Distinct from LF_LEVEL_UNSET so that
 *  re-entering a reaction is recognisable: `Reaction_get_level` walks the dependency graph
 *  recursively, and a cycle would otherwise find the level still unset and recurse into it
 *  again until the stack is gone. */
#define LF_LEVEL_IN_PROGRESS (-2)
typedef struct Reactor Reactor;
typedef struct Trigger Trigger;

struct Reaction {
  Reactor* parent;
  void (*body)(Reaction* self);
  void (*deadline_violation_handler)(Reaction* self);
  void (*stp_violation_handler)(Reaction* self);
  interval_t deadline;
  int level;
  /** True while this reaction is queued for the current tag. */
  bool _queued;
  // Intrusive link to the next reaction at the same level in the reaction
  // queue. The level list is circular, so a level holding a single reaction
  // points at itself. Queue-internal: only meaningful while this reaction is
  // enqueued, and left stale once it has been popped.
  Reaction* _next_in_level;
  size_t index;
  Trigger** effects;
  size_t effects_size;
  size_t effects_registered;
  size_t (*calculate_level)(Reaction* self);
  size_t (*get_level)(Reaction* self);
#if defined(LF_RUNTIME_EXTENSIONS)
  /** AND-composed gate consulted at the single enqueue funnel. */
  LfGate gate;
  /** Storage used by LF_REACTION_SET_GATE. */
  LfGateCondition _primary_gate;
#endif
};

void Reaction_ctor(Reaction* self, Reactor* parent, void (*body)(Reaction* self), Trigger** effects,
                   size_t effects_size, size_t index, void (*deadline_violation_handler)(Reaction*),
                   interval_t deadline, void (*stp_violation_handler)(Reaction*));

/**
 * @brief Whether this reaction can reach the reaction queue at all.
 */
static inline bool Reaction_may_enqueue(const Reaction* reaction) {
#if defined(LF_RUNTIME_EXTENSIONS)
  return LfGate_is_open(&reaction->gate);
#else
  (void)reaction;
  return true;
#endif
}

#endif
