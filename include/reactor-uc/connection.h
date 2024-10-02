#ifndef REACTOR_UC_CONNECTION_H
#define REACTOR_UC_CONNECTION_H

#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/action.h"

typedef struct Connection Connection;
typedef struct LogicalConnection LogicalConnection;
typedef struct PhysicalConnection PhysicalConnection;
typedef struct DelayedConnection DelayedConnection;
typedef struct Port Port;
typedef struct Output Output;

struct Connection {
  Trigger super;
  Port *upstream;     // Single upstream port
  Port **downstreams; // Pointer to array of pointers of downstream ports
  size_t downstreams_size;
  size_t downstreams_registered;
  // FIXME: Make into macro
  void (*register_downstream)(Connection *, Port *);
  // FIXME: Does not have to be a member?
  Output *(*get_final_upstream)(Connection *);
  /**
   * @brief Recursive function that traverses down the connection until it
   * reaches the final Inputs or Connections that schedule events.
   */
  void (*trigger_downstreams)(Connection *, const void *value_ptr, size_t value_size);
};

void Connection_ctor(Connection *self, TriggerType type, Reactor *parent, Port *upstream, Port **downstreams,
                     size_t num_downstreams, TriggerValue *trigger_value, void (*prepare)(Trigger *),
                     void (*cleanup)(Trigger *), void (*trigger_downstreams)(Connection *, const void *, size_t));

struct LogicalConnection {
  Connection super;
};

void LogicalConnection_ctor(LogicalConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                            size_t num_downstreams);

struct DelayedConnection {
  Connection super;
  interval_t delay;
  TriggerValue trigger_value; // FIFO for storing outstanding events
};

void DelayedConnection_ctor(DelayedConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                            size_t num_downstreams, interval_t delay, void *value_buf, size_t value_size,
                            size_t value_capacity);

struct PhysicalConnection {
  Connection super;
  interval_t delay;
  TriggerValue trigger_value;
};

void PhysicalConnection_ctor(PhysicalConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                             size_t num_downstreams, interval_t delay, void *value_buf, size_t value_size,
                             size_t value_capacity);
#endif
