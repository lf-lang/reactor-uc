// NOLINTBEGIN
/**
 * This file provides the implementation for a generic output. To use it
 * define type, number of sources and effects and a unique name for this action.
 * ```
 * #define T char`
 * #define N_EFFECTS 2`
 * #define NAME MyInput
 * #include "reactor-uc/generics/generic_input.h"
 * ```
 * Now we can instantiate such an input and construct it.
 */
#ifndef T
#error Template type T not defined
#endif

#ifndef NAME
#error NAME of the input not defined
#endif

#ifndef N_EFFECTS
#error NAME of the input not defined
#endif

#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

typedef struct NAME NAME;

struct NAME {
  InputPort super;
  Reaction *sources[N_EFFECTS];
  T value;
};

/**
 * @brief This will generate the function MyInput_ctor.
 *
 */
static void JOIN(NAME, ctor)(NAME *self, Reactor *parent) {
  InputPort_ctor(&self->super, INPUT, parent, NULL, 0, self->sources, N_EFFECTS, NULL);
}

#undef T
#undef NAME
#undef N_EFFECTS
// NOLINTEND