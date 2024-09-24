#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct InputPort InputPort;
typedef struct OutputPort OutputPort;
typedef struct Connection Connection;
typedef struct Port Port;

struct Port {
  Trigger super;
  TriggerValue trigger_value;
  Connection *conn_in;
  Connection *conn_out;
  void (*copy_value_and_trigger_downstreams)(Port *self, const void *value, size_t value_size);
};

struct InputPort {
  Port super;
  void (*trigger_effects)(InputPort *);
};

struct OutputPort {
  Port super;
};

void InputPort_ctor(InputPort *self, Reactor *parent, Reaction **effects, size_t effects_size, size_t value_size,
                    void *value_buf, size_t value_capacity);

void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size);
void Port_ctor(Port *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
               Reaction **sources, size_t sources_size, size_t value_size, void *value_buf, size_t value_capacity);

#endif
