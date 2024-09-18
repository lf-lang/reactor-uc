#ifndef REACTOR_UC_CONNECTION_H
#define REACTOR_UC_CONNECTION_H

#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Connection Connection;
typedef struct Port Port;
typedef struct OutputPort OutputPort;

struct Connection {
  Reactor *parent;
  Port *upstream;
  Port **downstreams;
  size_t downstreams_size;
  size_t downstreams_registered;
  void (*register_downstream)(Connection *, Port *);
  OutputPort *(*get_final_upstream)(Connection *);
};

void Connection_ctor(Connection *self, Reactor *parent, Port *upstream, Port **downstreams, size_t num_downstreams);

#endif
