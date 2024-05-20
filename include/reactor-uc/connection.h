#ifndef REACTOR_UC_CONNECTION_H
#define REACTOR_UC_CONNECTION_H

#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Connection Connection;

struct Connection {
  Reactor *parent;
  OutputPort *upstream;
  InputPort **downstreams;
  size_t downstreams_size;
  size_t downstreams_registered;
  void (*register_downstream)(Connection *, InputPort *);
};

void Connection_ctor(Connection *self, Reactor *parent, OutputPort *upstream, InputPort **downstreams,
                     size_t downstreams_size);

#endif
