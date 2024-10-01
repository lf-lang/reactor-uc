#ifndef REACTOR_UC_CONNECTION_H
#define REACTOR_UC_CONNECTION_H

#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Connection Connection;
typedef struct PhysicalConnection PhysicalConnection;
typedef struct DelayedConnection DelayedConnection;
typedef struct Port Port;
typedef struct OutputPort OutputPort;

typedef enum { CONN_LOGICAL, CONN_PHYSICAL, CONN_DELAYED } ConnectionType;

struct Connection {
  ConnectionType type;
  Reactor *parent;
  Port *upstream;
  Port **downstreams;
  size_t downstreams_size;
  size_t downstreams_registered;
  void (*register_downstream)(Connection *, Port *);
  OutputPort *(*get_final_upstream)(Connection *);
};

void Connection_ctor(Connection *self, ConnectionType type, Reactor *parent, Port *upstream, Port **downstreams,
                     size_t num_downstreams);

struct DelayedConnection {
  Connection super;
  Trigger trigger;
  interval_t delay;
  TriggerValue trigger_value;
};

void DelayedConnection_ctor(DelayedConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                            size_t num_downstreams, interval_t delay, void *value_buf, size_t value_size,
                            size_t value_capacity);

struct PhysicalConnection {
  Connection super;
  interval_t delay;
};

void PhysicalConnection_ctor(PhysicalConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                             size_t num_downstreams, interval_t delayl);
#endif
