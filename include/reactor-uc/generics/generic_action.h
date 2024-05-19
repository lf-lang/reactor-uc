// NOLINTBEGIN
/**
 * This file provides the implementation for a generic action. To use it
 * define type, number of sources and effects and a unique name for this action.
 * ```
 * #define T char
 * #define N_SOURCES 1
 * #define N_EFFECTS 2
 * #define NAME MyAction
 * #include "reactor-uc/generics/generic_action.h"
 * ```
 * Now we can instantiate such an action and construct it.
 */
#ifndef T
#error Template type T not defined
#endif

#ifndef N_SOURCES
#error N_SOURCES of the action not defined
#endif

#ifndef N_EFFECTS
#error N_EFFECTS of the action not defined
#endif

#ifndef NAME
#error NAME of the action not defined
#endif

#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

typedef struct NAME NAME;

struct NAME {
  Trigger super;
  bool is_present;
  T value;
  T next_value;
  Reaction *sources[N_SOURCES];
  Reaction *effects[N_EFFECTS];
};

/**
 * @brief This will generate the function MyAction_update_value which is invoked at the
 * start of the time step to update the value in the action to the next one.
 */
static void JOIN(NAME, update_value)(Trigger *_self) {
  NAME *self = (NAME *)_self;
  self->value = self->next_value;
}

/**
 * @brief This will generate the function MyAction_ctor.
 *
 */
static void JOIN(NAME, ctor)(NAME *self, Reactor *parent) {
  Trigger_ctor(&self->super, parent, self->effects, N_EFFECTS, self->sources, N_SOURCES, &JOIN(NAME, update_value));
}

#undef T
#undef NAME
#undef N_EFFECTS
#undef N_SOURCES
// NOLINTEND