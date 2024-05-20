// NOLINT
/**
 * This file provides the implementation for a generic output. To use it
 * define type, number of sources and effects and a unique name for this action.
 * ```
 * #define T char`
 * #define N_SOURCES 2`
 * #define NAME MyOutput
 * #include "reactor-uc/generics/generic_output.h"
 * ```
 * Now we can instantiate such an output and construct it.
 */
#ifndef T
#error Template type T not defined
#endif

#ifndef NAME
#error NAME of the output not defined
#endif

#ifndef N_SOURCES
#error NAME of the output not defined
#endif

#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

typedef struct NAME NAME;

struct NAME {
  OutputPort super;
  Reaction *sources[N_SOURCES];
  void (*set)(OutputPort *self, T value);
};

/**
 * @brief This will generate the function MyAction_update_value which is invoked at the
 * start of the time step to update the value in the action to the next one.
 */
static void JOIN(NAME, set)(NAME *self, T value) {
  Connection *conn = self->super.conn;
  if (!conn)
    return;

  for (int i = 0; i < conn->downstreams_registered; i++) {
    memcpy(conn->downstreams[i]->value_ptr, &value, sizeof(T));
    conn->downstreams[i]->trigger_reactions(conn->downstreams[i]);
  }
}

/**
 * @brief This will generate the function MyAction_ctor.
 *
 */
static void JOIN(NAME, ctor)(NAME *self, Reactor *parent) {
  Trigger_ctor(&self->super, OUTPUT, parent, self->sources, N_SOURCES, NULL, 0, NULL);
}

#undef T
#undef NAME
#undef N_SOURCES
