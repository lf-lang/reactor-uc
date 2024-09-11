#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct InputPort InputPort;
typedef struct OutputPort OutputPort;
typedef struct BasePort BasePort;

struct BasePort {
  // inward binding
  BasePort *inward_binding;

  // typed main
  void *typed;
};

struct InputPort {
  // methods
  BasePort *(*get)(InputPort *self);

  // member variables
  // Trigger super;

  BasePort base;
};

struct OutputPort {
  // methods
  void (*trigger_reactions)(OutputPort *self);

  // member variables
  Trigger super;
  BasePort base;
};

void BasePort_ctor(BasePort *self, void *typed);

void InputPort_ctor(InputPort *self, Reactor *parent, void *typed, Reaction **sources, size_t source_size);
void OutputPort_ctor(OutputPort *self, Reactor *parent, void *typed, Reaction **effects, size_t effect_size);

BasePort *InputPort_get(InputPort *self);
void InputPort_register_inward_binding(InputPort *self, BasePort *inward_binding);

void OutputPort_trigger_reactions(OutputPort *self);

#endif
