#include <stdio.h>
#include <stdlib.h>

#include "../../../reactor-uc/external/proto/message.pb.h"
#include "../../../reactor-uc/include/reactor-uc/encoding.h"

#define BUF_SIZE 1024
#define CONN_ID 42
int main() {

  PortMessage original_message;
  PortMessage deserialized_message;
  char buffer[BUF_SIZE];
  const char *message = NULL;
  int message_size = 0;

  original_message.connection_number = CONN_ID;
  const char *text = "Hello World1234";
  memcpy(original_message.message, text, sizeof("Hello World1234"));

  message = buffer;
  message_size = encode_protobuf(&original_message, buffer, BUF_SIZE);
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
