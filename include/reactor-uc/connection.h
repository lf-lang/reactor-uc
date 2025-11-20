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

typedef enum { LOGICAL_CONNECTION, PHYSICAL_CONNECTION } ConnectionType;

struct Connection {
  Trigger super;
  Port* upstream;                // Single upstream port
  Port** downstreams;            // Pointer to array of pointers of downstream ports
  size_t downstreams_size;       // Size of downstreams array
  size_t downstreams_registered; // Number of downstreams currently registered
  void (*register_downstream)(Connection*, Port*);
  Port* (*get_final_upstream)(Connection*);
  void (*trigger_downstreams)(Connection*, const void* value_ptr, size_t value_size);
};

void Connection_ctor(Connection* self, TriggerType type, Reactor* parent, Port** downstreams, size_t num_downstreams,
                     EventPayloadPool* payload_pool, void (*prepare)(Trigger*, Event*), void (*cleanup)(Trigger*),
                     void (*trigger_downstreams)(Connection*, const void*, size_t));

struct LogicalConnection {
  Connection super;
};

void LogicalConnection_ctor(LogicalConnection* self, Reactor* parent, Port** downstreams, size_t num_downstreams);

struct DelayedConnection {
  Connection super;
  interval_t delay;
  ConnectionType type;
  EventPayloadPool payload_pool;
  void* staged_payload_ptr;
};

void DelayedConnection_ctor(DelayedConnection* self, Reactor* parent, Port** downstreams, size_t num_downstreams,
                            interval_t delay, ConnectionType type, size_t payload_size, void* payload_buf,
                            bool* payload_used_buf, size_t payload_buf_capacity);

#endif
