#ifndef REACTOR_UC_SERIALIZATION_H
#define REACTOR_UC_SERIALIZATION_H

#include "proto/message.pb.h"
#include "reactor-uc/error.h"
#include <stddef.h>

// The maximum size of a serialized payload. 
// NOTE: This MUST match the max size of the payload in the protobuf message definition.
#ifndef SERIALIZATION_MAX_PAYLOAD_SIZE
#define SERIALIZATION_MAX_PAYLOAD_SIZE 832
#endif

int serialize_to_protobuf(const FederateMessage *message, unsigned char *buffer, size_t buffer_size);
int deserialize_from_protobuf(FederateMessage *message, const unsigned char *buffer, size_t buffer_size);

lf_ret_t deserialize_payload_default(void *user_struct, const unsigned char *msg_buf, size_t msg_size);

ssize_t serialize_payload_default(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf);
#endif // REACTOR_UC_SERIALIZATION_H
