#include <stdio.h>
#include <stdlib.h>

#include <nanopb/pb_encode.h>
#include <nanopb/pb_decode.h>

#include "../../../reactor-uc/include/reactor-uc/generated/message.pb.h"
#include "../../../reactor-uc/include/reactor-uc/encoding.h"
int main() {

  PortMessage original_message;
  PortMessage deserialized_message;
  unsigned char buffer[1024];
  const unsigned char * message = NULL;
  int message_size = 0;

  original_message.connection_number = 42;
  const char* text = "Hello World1234";
  memcpy(original_message.message, text, sizeof("Hello World1234"));

  message = buffer;
  message_size = encode_protobuf(&original_message, buffer, 1024);
  if (message_size < 0) {
    printf("encoding failed!\n");
    exit(1);
  }

  int remaining_bytes = decode_protobuf(&deserialized_message, message, message_size);

  if (remaining_bytes < 0) {
    printf("decoding failed!\n");
    exit(1);
  }

  printf("o: %i d: %i\n", original_message.connection_number, deserialized_message.connection_number);
  printf("o: %s d: %s\n", original_message.message, deserialized_message.message);
}
