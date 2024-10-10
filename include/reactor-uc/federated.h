#ifndef REACTOR_UC_FEDERATED_H
#define REACTOR_UC_FEDERATED_H

// TODO: We dont want to include a generic network_bundle here. Not posix specific
#include "reactor-uc/connection.h"
#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include "reactor-uc/reactor.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct FederatedOutputConnection FederatedOutputConnection;
typedef struct FederatedInputConnection FederatedInputConnection;

// Wrapping all connections going both ways between this federated and
// another federated of.
struct FederatedConnectionBundle {
  Reactor *parent; // Pointer to the federate
  TcpIpBundle *net_bundle;
  FederatedInputConnection **inputs;
  size_t inputs_size;
  FederatedOutputConnection **outputs;
  size_t outputs_size;
  bool server; // Does this federate work as server or client
};

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, TcpIpBundle *net_bundle,
                                    FederatedInputConnection **inputs, size_t inputs_size,
                                    FederatedOutputConnection **outputs, size_t outputs_size);

// A single output connection from this federate. Might connect to several
// downstream ports, but all of them must be in the same federate
struct FederatedOutputConnection {
  Connection super;                  // Inherits from Connection, it wastes some memory but makes for a nicer arch
  FederatedConnectionBundle *bundle; // A pointer to the bundle it is within
  void *value_ptr;                   // Pointer to the buffer where the output value is staged before being sent
  size_t value_size;                 // Size of an output value
  bool staged;                       // Has an output been staged for transmission?
  int conn_id;
};

void FederatedOutputConnection_ctor(FederatedOutputConnection *self, Reactor *parent, FederatedConnectionBundle *bundle,
                                    int conn_id, Port *upstream, void *value_ptr, size_t value_size);

// A single input connection to this federate. Has a single upstream port
struct FederatedInputConnection {
  Connection super;
  interval_t delay;                // The delay of this connection
  bool is_physical;                // Is the connection physical?
  tag_t last_known_tag;            // The latest tag this input is known at.
  instant_t safe_to_assume_absent; //
  TriggerValue trigger_value;
  int conn_id;
  void (*schedule)(FederatedInputConnection *self, TaggedMessage *msg);
};

void FederatedInputConnection_ctor(FederatedInputConnection *self, Reactor *parent, interval_t delay, bool is_physical,
                                   Port **downstreams, size_t downstreams_size, void *value_buf, size_t value_size,
                                   size_t value_capacity);
#endif