#include "reactor-uc/serialization.h"

#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>

#ifdef MIN
#undef MIN
#endif
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int serialize_to_protobuf(const FederateMessage *message, unsigned char *buffer, size_t buffer_size) {
  // turing write buffer into pb_ostream buffer
  pb_ostream_t stream_out = pb_ostream_from_buffer(buffer, buffer_size);

  // serializing protobuf into buffer
  if (!pb_encode_delimited(&stream_out, FederateMessage_fields, message)) {
    return -1;
  }

  return (int)stream_out.bytes_written;
}

int deserialize_from_protobuf(FederateMessage *message, const unsigned char *buffer, size_t buffer_size) {
  pb_istream_t stream_in = pb_istream_from_buffer(buffer, buffer_size);

  if (!pb_decode_delimited(&stream_in, FederateMessage_fields, message)) {
    return -1;
  }

  return (int)stream_in.bytes_left;
}

lf_ret_t deserialize_payload_default(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  if (msg_size > SERIALIZATION_MAX_PAYLOAD_SIZE) {
    return LF_ERR;
  }
  memcpy(user_struct, msg_buf, msg_size);
  return LF_OK;
}

int serialize_payload_default(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  if (user_struct_size > SERIALIZATION_MAX_PAYLOAD_SIZE) {
    return -1;
  }
  memcpy(msg_buf, user_struct, user_struct_size);
  return user_struct_size;
}

int generate_message_framing(unsigned char *buffer, size_t message_size, EncryptionIdentifier encryption_identifier) {
  MessageFraming *frame = (MessageFraming *)buffer;
  frame->preamble = 0xAA;
  frame->protocol_version = 0x0;
  frame->message_size = message_size;
  frame->crypto_id = encryption_identifier;

  return sizeof(MessageFraming);
}

lf_ret_t validate_message_framing(unsigned char *buffer, EncryptionIdentifier expected_encryption_id) {
  MessageFraming *frame = (MessageFraming *)buffer;

<<<<<<< HEAD
  if (frame->preamble != 0xAA || frame->protocol_version != 0x0 || frame->crypto_id != expected_encryption_id) {
=======
  if (frame->preamble != 0xBEEF || frame->protocol_version != 0x0 || frame->crypto_id != expected_encryption_id) {
>>>>>>> 8c31f12 (adding codegen)
    return LF_ERR;
  }

  return LF_OK;
}
