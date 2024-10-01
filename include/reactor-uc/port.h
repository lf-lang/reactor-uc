#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Input Input;
typedef struct Output Output;
typedef struct Connection Connection;
typedef struct Port Port;

struct Port {
  Trigger super;
  Connection *conn_in;
  Connection *conn_out;
};

struct Input {
  Port super;
  TriggerEffects effects;
  void *value_ptr;
  size_t value_size;
};

struct Output {
  Port super;
  TriggerSources sources;
};

void Input_ctor(Input *self, Reactor *parent, Reaction **effects, size_t effects_size, void *value_ptr,
                size_t value_size);

void Output_ctor(Output *self, Reactor *parent, Reaction **sources, size_t sources_size);

void Port_ctor(Port *self, TriggerType type, Reactor *parent, void (*prepare)(Trigger *), void (*cleanup)(Trigger *),
               const void *(*get)(Trigger *));

#endif
