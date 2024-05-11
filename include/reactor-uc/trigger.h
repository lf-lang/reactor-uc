#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stddef.h>

typedef struct Trigger Trigger;

typedef void (*Trigger_start_time_step)(Trigger *self);
struct Trigger {
  Reactor *parent;
  Reaction **effects;
  size_t effects_size;
  Reaction **sources;
  size_t sources_size;
  Trigger_start_time_step start_time_step;
  void (*schedule_at)(Trigger *self, tag_t tag);
};

void Trigger_ctor(Trigger *self, Reactor *parent, Reaction **effects, size_t effects_size, Reaction **sources,
                  size_t sources_size, Trigger_start_time_step start_time_step);

#endif
