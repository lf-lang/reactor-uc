#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Connection Connection;
typedef struct Port Port;

struct Port {
  Trigger super;
  void *value_ptr;             // Pointer to the `buffer` field in the user Input port struct.
  size_t value_size;           // Size of the data stored in this Port.
  TriggerEffects effects;      // The reactions triggered by this Port
  TriggerSources sources;      // The reactions that can write to this Port.
  TriggerObservers observers;  // The reactions that can observe this Port.
  Connection *conn_in;         // Connection coming into the port.
  Connection **conns_out;      // Connections going out of the port.
  size_t conns_out_size;       // Number of connections going out of the port.
  size_t conns_out_registered; // Number of connections that have been registered for cleanup.
};

// Output ports need pointers to arrats of effects, observers and connections which are not
// located within the same reactor, but in the parent reactor. This struct has all the arguments
// that are needed to initialize an Output port. This is only used in the code-generated code.
typedef struct {
  Reaction **parent_effects;
  size_t parent_effects_size;
  Reaction **parent_observers;
  size_t parent_observers_size;
  Connection **conns_out;
  size_t conns_out_size;
} OutputExternalCtorArgs;


// Likewise, Input ports need pointers to arrays of sources, which are not located within the same reactor
// this struct captures all those arguments.
typedef struct {
  Reaction **parent_sources;
  size_t parent_sources_size;
} InputExternalCtorArgs;

void Port_ctor(Port *self, TriggerType type, Reactor *parent, void *value_ptr, size_t value_size, Reaction **effects,
               size_t effects_size, Reaction **sources, size_t sources_size, Reaction **observers,
               size_t observers_size, Connection **conns_out, size_t conns_out_size);

#endif
