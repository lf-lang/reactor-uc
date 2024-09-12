#ifndef REACTOR_UC_BUILTIN_TRIGGERS_H
#define REACTOR_UC_BUILTIN_TRIGGERS_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Startup Startup;
typedef struct LogicalAction LogicalAction;

struct Startup {
  Trigger super;
};

void Startup_ctor(Startup *self, Reactor *parent, Reaction **effects, size_t effects_size);

struct LogicalAction {
  Trigger super;

  TriggerReply (*schedule)(LogicalAction *self, interval_t in);
};

void LogicalAction_ctor(LogicalAction *self, Reactor *parent, Reaction **effects, size_t effects_size,
                        Reaction **sources, size_t source_size);

TriggerReply LogicalAction_schedule(LogicalAction *self, interval_t in);

#endif
