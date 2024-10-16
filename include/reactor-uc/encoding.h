#ifndef REACTOR_UC_ENCODING_H
#define REACTOR_UC_ENCODING_H

#include "proto/message.pb.h"
#include <nanopb/pb_decode.h>
#include <nanopb/pb_encode.h>

int encode_protobuf(const TaggedMessage *message, unsigned char *buffer, size_t buffer_size) {
  // turing write buffer into pb_ostream buffer
  pb_ostream_t stream_out = pb_ostream_from_buffer(buffer, buffer_size);

  // serializing protobuf into buffer
  if (!pb_encode(&stream_out, TaggedMessage_fields, message)) {
    printf("protobuf encoding error %s\n", stream_out.errmsg);
    return -1;
  }

  return (int)stream_out.bytes_written;
}

int decode_protobuf(TaggedMessage *message, const unsigned char *buffer, size_t buffer_size) {
  pb_istream_t stream_in = pb_istream_from_buffer(buffer, buffer_size);

  if (!pb_decode(&stream_in, TaggedMessage_fields, message)) {
    return -1;
  }

  return (int)stream_in.bytes_left;
}

#endif // REACTOR_UC_ENCODING_H
