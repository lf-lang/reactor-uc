#include "reactor-uc/serialization.h"

#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>

#ifdef MIN
#undef MIN
#endif
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int serialize_to_protobuf(const FederateMessage* message, unsigned char* buffer, size_t buffer_size) {
  // turing write buffer into pb_ostream buffer
  pb_ostream_t stream_out = pb_ostream_from_buffer(buffer, buffer_size);

  // serializing protobuf into buffer
  if (!pb_encode_delimited(&stream_out, FederateMessage_fields, message)) {
    return -1;
  }

  return (int)stream_out.bytes_written;
}

int deserialize_from_protobuf(FederateMessage* message, const unsigned char* buffer, size_t buffer_size) {
  pb_istream_t stream_in = pb_istream_from_buffer(buffer, buffer_size);

  if (!pb_decode_delimited(&stream_in, FederateMessage_fields, message)) {
    return -1;
  }

  return (int)stream_in.bytes_left;
}

lf_ret_t deserialize_payload_default(void* user_struct, size_t user_struct_size, const unsigned char* msg_buf,
                                     size_t msg_size) {
  // The default serializer sends the raw port value, so a message of any other size was sent by a
  // federate with a different port type (e.g. a different build) and cannot be copied.
  if (msg_size > SERIALIZATION_MAX_PAYLOAD_SIZE || msg_size != user_struct_size) {
    return LF_ERR;
  }
  if (msg_size > 0) {
    memcpy(user_struct, msg_buf, msg_size);
  }
  return LF_OK;
}

int serialize_payload_default(const void* user_struct, size_t user_struct_size, unsigned char* msg_buf) {
  if (user_struct_size > SERIALIZATION_MAX_PAYLOAD_SIZE) {
    return -1;
  }
  memcpy(msg_buf, user_struct, user_struct_size);
  return user_struct_size;
}
