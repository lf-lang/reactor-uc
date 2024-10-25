#ifndef REACTOR_UC_FEDERATED_H
#define REACTOR_UC_FEDERATED_H

#include "reactor-uc/connection.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/reactor.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct FederatedOutputConnection FederatedOutputConnection;
typedef struct FederatedInputConnection FederatedInputConnection;
typedef struct NetworkChannel NetworkChannel;

// returns how many bytes of the buffer were used by the serialized string
typedef size_t (*serialize_hook)(const void *user_struct, size_t user_struct_size, unsigned char *msg_buffer);

// returns if the deserialization was successful
typedef lf_ret_t (*deserialize_hook)(void *user_struct, const unsigned char *msg_buffer, size_t msg_size);

// Wrapping all connections going both ways between this federated and
// another federated of.
struct FederatedConnectionBundle {
  Reactor *parent;             // Pointer to the federate
  NetworkChannel *net_channel; // Pointer to the network super doing the actual I/O
  // Pointer to an array of input connections which should live in the derived struct.
  FederatedInputConnection **inputs;
  deserialize_hook *deserialize_hooks;
  size_t inputs_size;

  // Pointer to an array of output connections which should live in the derived struct.
  FederatedOutputConnection **outputs;
  serialize_hook *serialize_hooks;
  size_t outputs_size;
  bool server; // Does this federate work as server or client
};

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, NetworkChannel *net_channel,
                                    FederatedInputConnection **inputs, deserialize_hook *deserialize_hooks,
                                    size_t inputs_size, FederatedOutputConnection **outputs,
                                    serialize_hook *serialize_hooks, size_t outputs_size);

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

void Federated_distribute_start_tag(Environment *env, instant_t start_time);
#endif