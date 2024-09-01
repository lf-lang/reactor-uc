#ifndef REACTOR_UC_CONNECTION_H
#define REACTOR_UC_CONNECTION_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/port.h"

typedef struct Connection Connection;
typedef struct InputPort InputPort;
typedef struct OutputPort OutputPort;

// OutputPort ---- Properties ----> InputPort
// this struct is primarily used to save the dependencies
// between ports and therefore also reactions
struct Connection {
  // methods
  void (*register_downstream)(Connection *, InputPort *);

  // member variables
  Reactor *parent_;
  OutputPort *upstream_;
  InputPort *downstream_;
  int64_t delay_;
};

void Connection_ctor(Connection *self, Reactor *parent, OutputPort *upstream, InputPort *downstream, int64_t delay);

#endif
