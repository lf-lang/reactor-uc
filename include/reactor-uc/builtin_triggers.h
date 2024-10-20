#ifndef REACTOR_UC_BUILTIN_TRIGGERS_H
#define REACTOR_UC_BUILTIN_TRIGGERS_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct BuiltinTrigger BuiltinTrigger;

struct BuiltinTrigger {
  Trigger super;
  TriggerEffects effects;
  BuiltinTrigger *next;
};

void BuiltinTrigger_ctor(BuiltinTrigger *self, TriggerType type, Reactor *parent, Reaction **effects,
                         size_t effects_size);

#endif
