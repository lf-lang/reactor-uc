#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct InputPort InputPort;
typedef struct OutputPort OutputPort;
typedef struct Connection Connection;

struct InputPort {
  Trigger super;
  Connection *conn;
  void **value_ptr_ptr; // Points to `*value_ptr` of code-gen InputPort struct
};

struct OutputPort {
  Trigger super;
  Connection *conn;
  void *value_ptr;
  void (*trigger_downstreams)(OutputPort *self);
};

void InputPort_ctor(InputPort *self, Reactor *parent, Reaction **effects, size_t effects_size, void **value_ptr_ptr);
void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size, void *value_ptr);

#endif
