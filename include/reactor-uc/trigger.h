#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stddef.h>

typedef struct Trigger Trigger;

typedef void (*Trigger_update_value)(Trigger *self);
struct Trigger {
  Reactor *parent;
  Reaction **effects;
  size_t effects_registered;
  size_t effects_size;
  Reaction **sources;
  size_t sources_size;
  size_t sources_registered;
  Trigger_update_value update_value;
  void (*schedule_at)(Trigger *, tag_t);
  void (*register_effect)(Trigger *, Reaction *);
  void (*register_source)(Trigger *, Reaction *);
};

void Trigger_ctor(Trigger *self, Reactor *parent, Reaction **effects, size_t effects_size, Reaction **sources,
                  size_t sources_size, Trigger_update_value update_value_func);

#endif
