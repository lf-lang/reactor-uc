#ifndef REACTOR_UC_CONNECTION_H
#define REACTOR_UC_CONNECTION_H

#include "reactor-uc/action.h"
#include "reactor-uc/error.h"
#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Connection Connection;
typedef struct LogicalConnection LogicalConnection;
typedef struct PhysicalConnection PhysicalConnection;
typedef struct DelayedConnection DelayedConnection;
typedef struct Port Port;
typedef struct Output Output;

struct Connection {
  Trigger super;
  Port *upstream;                // Single upstream port
  Port **downstreams;            // Pointer to array of pointers of downstream ports
  size_t downstreams_size;       // Size of downstreams array
  size_t downstreams_registered; // Number of downstreams currently registered
  /**
   * @brief Register a downstream port to this connection. Will increase the downstreams_registered counter.
   */
  void (*register_downstream)(Connection *, Port *);
  /**
   * @brief Returns the eventual upstream Output port that drives the possible chain of connections that
   * this connection is part of.
   */
  Output *(*get_final_upstream)(Connection *);
  /**
   * @brief Recursive function that traverses down the connection until it
   * reaches the final Inputs or Connections that schedule events.
   */
  void (*trigger_downstreams)(Connection *, const void *value_ptr, size_t value_size);
};

/**
 * @brief Construct a Connection object. A Connection is an abstract type that can be either a Logical, Delayed,
 * Physical
 *
 * @param self The Connection object to construct
 * @param type The type of the connection. Either logical, delayed or physical.
 * @param parent The reactor in which this connection appears (not the reactors of the ports it connects)
 * @param upstream The pointer to the upstream port of this connection
 * @param downstreams A pointer to an array of pointers to downstream ports.
 * @param num_downstreams The size of the downstreams array.
 * @param trigger_value A pointer to the TriggerValue that holds the data of the events that are scheduled on this
 * connection.
 * @param prepare The prepare function that is called before the connection triggers its downstreams.
 * @param cleanup The cleanup function that is called at the end of timestep after all reactions have executed.
 * @param trigger_downstreams The function that triggers all downstreams of this connection.
 */
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
  TriggerValue trigger_value;
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
