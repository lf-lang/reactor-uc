#ifndef REACTOR_UC_SERIALIZATION_H
#define REACTOR_UC_SERIALIZATION_H

#include "proto/message.pb.h"
#include "reactor-uc/error.h"

int serialize_to_protobuf(const FederateMessage *message, unsigned char *buffer, size_t buffer_size);
int deserialize_from_protobuf(FederateMessage *message, const unsigned char *buffer, size_t buffer_size);

int serialize_message(const FederateMessage *message, unsigned char *buffer, size_t buffer_size);
int deserialize_message(FederateMessage *message, const unsigned char *buffer, size_t buffer_size);

lf_ret_t deserialize_payload_default(void *user_struct, const unsigned char *msg_buf, size_t msg_size);

size_t serialize_payload_default(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf);
#endif // REACTOR_UC_SERIALIZATION_H
