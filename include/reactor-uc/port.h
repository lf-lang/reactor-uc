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

typedef enum { INPUT, OUTPUT } PortType;

struct Port {
  PortType type;
  Reactor *parent;
  Connection *conn_in;
  Connection *conn_out;
  void (*copy_value_and_schedule_downstreams)(Port *self, const void *value);
};

struct Input {
  Port super;
  Reaction **effects;
  size_t effects_size;
  size_t effects_registered;
  bool is_present;
  void *value_ptr;
  size_t value_size;

  void (*prepare)(Input *);
};

struct Output {
  Port super;
  Reaction **sources;
  size_t sources_size;
  size_t sources_registered;
  bool is_present;
  void *value_ptr;
  size_t value_size;
};

void Input_ctor(Input *self, Reactor *parent, Reaction **effects, size_t effects_size, void *value_ptr,
                size_t value_size);

void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size, void *value_ptr,
                     size_t value_size);

void Port_ctor(Port *self, PortType type, Reactor *parent);

#endif
