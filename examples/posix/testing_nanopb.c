#include <stdio.h>
#include <stdlib.h>

#include <nanopb/pb_encode.h>
#include <nanopb/pb_decode.h>

#include "../../../reactor-uc/include/reactor-uc/generated/message.pb.h"
int main() {

  PortMessage original_message;
  PortMessage deserialized_message;
  unsigned char buffer[1024];
  const unsigned char * message = NULL;
  int message_size = 0;

  original_message.connection_number = 42;
  const char* text = "Hello World1234";
  memcpy(original_message.message, text, sizeof("Hello World1234"));

  // turing write buffer into pb_ostream buffer
  pb_ostream_t stream_out = pb_ostream_from_buffer(buffer, 1024);

  // serializing protobuf into buffer
  int status = pb_encode(&stream_out, PortMessage_fields, &original_message);

  if (status < 0) {
    printf("encoding failed!\n");
    exit(1);
  }

  message_size = stream_out.bytes_written;
  message = buffer;

  pb_istream_t stream_in = pb_istream_from_buffer(message, message_size);

  if(!pb_decode(&stream_in, PortMessage_fields, &deserialized_message)) {
    printf("decoding failed: %s\n", stream_in.errmsg);
    exit(1);
  }

  printf("o: %i d: %i\n", original_message.connection_number, deserialized_message.connection_number);
  printf("o: %s d: %s\n", original_message.message, deserialized_message.message);


}
