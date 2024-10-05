#include <stdio.h>
#include <stdlib.h>

#include "../../../reactor-uc/include/reactor-uc/encoding.h"
#include "../../external/proto/message.pb.h"

#define BUFFER_SIZE 1024
#define MSG_ID 42

int main() {

  PortMessage original_message;
  PortMessage deserialized_message;
  unsigned char buffer[BUFFER_SIZE];
  unsigned char * message = NULL;
  int message_size = 0;

  original_message.connection_number = MSG_ID;
  const char* text = "Hello World1234";
  memcpy(original_message.message, text, sizeof("Hello World1234")); // NOLINT

  message = buffer;
  message_size = encode_protobuf(&original_message, buffer, BUFFER_SIZE);
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
