#ifndef REACTOR_UC_FEDERATED_H
#define REACTOR_UC_FEDERATED_H

#include "reactor-uc/connection.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/reactor.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct FederatedOutputConnection FederatedOutputConnection;
typedef struct FederatedInputConnection FederatedInputConnection;
typedef struct NetworkChannel NetworkChannel;
typedef struct EncryptionLayer EncryptionLayer;

/**
 * @brief A function type for serializers that takes port values and serializes them into a message buffer.
 *
 * The default implementation is a raw memcpy, but this can be overridden with a custom implementation,
 * or with protobufs.
 *
 * @param user_struct A pointer to the port value to serialize
 * @param user_struct_size The size of the port value to serialize
 * @param msg_buffer A pointer to the buffer to write the serialized message to
 * @return The size of the serialized message
 */
typedef int (*serialize_hook)(const void *user_struct, size_t user_struct_size, unsigned char *msg_buffer);

/**
 * @brief A function type for deserializers that takes a message buffer and deserializes them to a port value.
 *
 * The default implementation is a raw memcpy, but this can be overridden with a custom implementation,
 * or with protobufs.
 *
 * @param user_struct A pointer to the port value into which the message should be deserialized
 * @param msg_buffer A pointer to the buffer with the serialized message
 * @param msg_size The size of the serialized message
 * @return Whether the deserialization was successful
 */
typedef lf_ret_t (*deserialize_hook)(void *user_struct, const unsigned char *msg_buffer, size_t msg_size);

/**
 * @brief A FederatedConnectionBundle is a collection of input and output connections to a single other federate.
 *
 * These connections are all multiplexed onto a single NetworkChannel. The bundle also contains the pointers to
 * the serializers and deserializers for each connection.
 */
struct FederatedConnectionBundle {
  Reactor *parent;                   // Pointer to the federate
  EncryptionLayer *encryption_layer; // Pointer to the network super doing the actual I/O
  // Pointer to an array of input connections which should live in the derived struct.
  FederatedInputConnection **inputs;
  deserialize_hook *deserialize_hooks;
  size_t inputs_size;

  // Pointer to an array of output connections which should live in the derived struct.
  FederateMessage send_msg;
  FederatedOutputConnection **outputs;
  serialize_hook *serialize_hooks;
  size_t outputs_size;
  bool server;  // Does this federate work as server or client
  size_t index; // Index of this FederatedConnectionBundle in the Environment's net_bundles array
};

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, EncryptionLayer* encryption_layer, NetworkChannel *net_channel,
                                    FederatedInputConnection **inputs, deserialize_hook *deserialize_hooks,
                                    size_t inputs_size, FederatedOutputConnection **outputs,
                                    serialize_hook *serialize_hooks, size_t outputs_size, size_t index);


/**
 * @brief A single output connection from this federate to another federate.
 *
 * This connection has a single upstream output port in the current federate, but might be connected to multiple
 * input ports in the downstream federate. But all input ports must be in the same federate.
 *
 */
struct FederatedOutputConnection {
  Connection super; // Inherits from Connection, it wastes some memory but makes for a nicer architecture.
  FederatedConnectionBundle *bundle; // A pointer to the super it is within
  EventPayloadPool payload_pool;     // Output buffer
  void *staged_payload_ptr;
  int conn_id;
};

void FederatedConnectionBundle_validate(FederatedConnectionBundle *bundle);

void FederatedOutputConnection_ctor(FederatedOutputConnection *self, Reactor *parent, FederatedConnectionBundle *bundle,
                                    int conn_id, void *payload_buf, bool *payload_used_buf, size_t payload_size,
                                    size_t payload_buf_capacity);

/**
 * @brief A single input connection coming from another federate.
 *
 * This connection has a single upstream output port in the other federate, but might be connected to multiple input
 * ports in the current federate.
 *
 */
struct FederatedInputConnection {
  Connection super;
  interval_t delay;     // The amount of delay on this connection
  ConnectionType type;  // Whether this is a logical or physical connection
  tag_t last_known_tag; // The latest tag this input is known at.
  instant_t max_wait;   // The maximum time we are willing to wait for this input to become known at any given tag.
  EventPayloadPool payload_pool;
  int conn_id;
  /**
   * @brief Schedule a received message on this input connection
   *
   * This is called by the network channel when a message is received.
   */
  void (*schedule)(FederatedInputConnection *self, TaggedMessage *msg);
};

void FederatedInputConnection_ctor(FederatedInputConnection *self, Reactor *parent, interval_t delay, bool is_physical,
                                   interval_t max_wait, Port **downstreams, size_t downstreams_size, void *payload_buf,
                                   bool *payload_used_buf, size_t payload_size, size_t payload_buf_capacity);

#endif
