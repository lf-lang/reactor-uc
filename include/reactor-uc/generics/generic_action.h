// NOLINTBEGIN
#ifndef T
#error Template type T not defined
#endif

#ifndef N_SOURCES
#error N_SOURCES not defined
#endif

#ifndef N_EFFECTS
#error N_EFFECTS not defined
#endif

#ifndef NAME
#error NAME not defined
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

static void JOIN(NAME, start)(Trigger *_self) {
  NAME *self = (NAME *)_self;
  self->value = self->next_value;
}

static void JOIN(NAME, ctor)(NAME *self, Reactor *parent) {
  Trigger_ctor(&self->super, parent, self->effects, N_EFFECTS, self->sources, N_SOURCES, &JOIN(NAME, start));
}

#undef T
#undef NAME
#undef N_EFFECTS
#undef N_SOURCES
// NOLINTEND