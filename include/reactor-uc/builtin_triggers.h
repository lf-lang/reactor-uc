#ifndef REACTOR_UC_BUILTIN_TRIGGERS_H
#define REACTOR_UC_BUILTIN_TRIGGERS_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Startup Startup;
typedef struct Shutdown Shutdown;

struct Startup {
  Trigger super;          // Inherit from Trigger
  TriggerEffects effects; // The reactions triggered by this Startup.
  Startup *next;          // Used to trigger all startup-reactions with only a single startup-event on the event queue.
};

void Startup_ctor(Startup *self, Reactor *parent, Reaction **effects, size_t effects_size);

struct Shutdown {
  Trigger super;          // Inherit from Trigger
  TriggerEffects effects; // The reactions triggered by this Shutdown.
  Shutdown *next; // Used to trigger all shutdown-reactions with only a single shutdown-event on the event queue.
};

void Shutdown_ctor(Shutdown *self, Reactor *parent, Reaction **effects, size_t effects_size);
#endif
