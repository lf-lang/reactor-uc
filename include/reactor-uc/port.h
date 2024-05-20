#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct InputPort InputPort;
typedef struct OutputPort OutputPort;

struct InputPort {
  Trigger super;
  void *value_ptr;
  Connection *conn;
  void (*trigger_reactions)(InputPort *self);
};

struct OutputPort {
  Trigger super;
  Connection *conn;
};

void InputPort_ctor(InputPort *self, Reactor *parent, void *value_ptr, Reaction **effects, size_t effects_size);
void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size);

#endif
