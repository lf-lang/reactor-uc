#ifndef REACTOR_UC_FEDERATED_H
#define REACTOR_UC_FEDERATED_H

#include "reactor-uc/connection.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/reactor.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct FederatedOutputConnection FederatedOutputConnection;
typedef struct FederatedInputConnection FederatedInputConnection;
typedef struct NetworkChannel NetworkChannel;

// Wrapping all connections going both ways between this federated and
// another federated of.
struct FederatedConnectionBundle {
  Reactor *parent;             // Pointer to the federate
  NetworkChannel *net_channel; // Pointer to the network super doing the actual I/O
  // Pointer to an array of input connections which should live in the derived struct.
  FederatedInputConnection **inputs;
  size_t inputs_size;
  // Pointer to an array of output connections which should live in the derived struct.
  FederatedOutputConnection **outputs;
  size_t outputs_size;
  bool server; // Does this federate work as server or client
};

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, NetworkChannel *net_channel,
                                    FederatedInputConnection **inputs, size_t inputs_size,
                                    FederatedOutputConnection **outputs, size_t outputs_size);

// A single output connection from this federate. Might connect to several
// downstream ports, but all of them must be in the same federate
struct FederatedOutputConnection {
  Connection super;                  // Inherits from Connection, it wastes some memory but makes for a nicer arch
  FederatedConnectionBundle *bundle; // A pointer to the super it is within
  EventPayloadPool payload_pool;     // Output buffer
  void *staged_payload_ptr;
  int conn_id;
};

void FederatedOutputConnection_ctor(FederatedOutputConnection *self, Reactor *parent, FederatedConnectionBundle *bundle,
                                    int conn_id, void *payload_buf, bool *payload_used_buf, size_t payload_size,
                                    size_t payload_buf_capacity);

// A single input connection to this federate. Has a single upstream port
struct FederatedInputConnection {
  Connection super;
  interval_t delay; // The delay of this connection
  ConnectionType type;
  tag_t last_known_tag; // The latest tag this input is known at.
  instant_t safe_to_assume_absent;
  EventPayloadPool payload_pool;
  int conn_id;
  void (*schedule)(FederatedInputConnection *self, TaggedMessage *msg);
};

void FederatedInputConnection_ctor(FederatedInputConnection *self, Reactor *parent, interval_t delay,
                                   ConnectionType type, Port **downstreams, size_t downstreams_size, void *payload_buf,
                                   bool *payload_used_buf, size_t payload_size, size_t payload_buf_capacity);
#endif