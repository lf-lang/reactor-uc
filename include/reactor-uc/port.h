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
  Connection *conn_in;  // Connection coming into the port.
  Connection *conn_out; // Connection going out of the port.
};

struct Input {
  Port super;
  TriggerEffects effects; // The reactions triggered by this Input port.
  void *value_ptr;        // Pointer to the `buffer` field in the user Input port struct.
  size_t value_size;      // Size of the data stored in this Input Port.
};

struct Output {
  Port super;
  TriggerSources sources; // The reactions that can write to this Output port.
};

void Input_ctor(Input *self, Reactor *parent, Reaction **effects, size_t effects_size, void *value_ptr,
                size_t value_size);

void Output_ctor(Output *self, Reactor *parent, Reaction **sources, size_t sources_size);

void Port_ctor(Port *self, TriggerType type, Reactor *parent, void (*prepare)(Trigger *, Event *),
               void (*cleanup)(Trigger *));

#endif
