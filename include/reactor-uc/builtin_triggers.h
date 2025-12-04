#ifndef REACTOR_UC_BUILTIN_TRIGGERS_H
#define REACTOR_UC_BUILTIN_TRIGGERS_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct BuiltinTrigger BuiltinTrigger;

struct BuiltinTrigger {
  Trigger super;
  TriggerEffects effects;
  TriggerObservers observers;
  BuiltinTrigger* next;
};

void BuiltinTrigger_ctor(BuiltinTrigger* self, TriggerType type, Reactor* parent, Reaction** effects,
                         size_t effects_size, Reaction** observers, size_t observers_size);

#endif
