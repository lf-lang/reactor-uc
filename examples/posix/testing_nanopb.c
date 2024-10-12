#include <stdio.h>
#include <stdlib.h>

#include "proto/message.pb.h"
#include "reactor-uc/encoding.h"

#define BUFFER_SIZE 1024
#define MSG_ID 42

int main() {

  TaggedMessage original_message;
  TaggedMessage deserialized_message;
  unsigned char buffer[BUFFER_SIZE];
  unsigned char *message = NULL;
  int message_size = 0;

  original_message.conn_id = MSG_ID;
  const char *text = "Hello World1234";
  memcpy(original_message.payload.bytes, text, sizeof("Hello World1234")); // NOLINT
  original_message.payload.size = sizeof("Hello World1234");

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

  printf("o: %i d: %i\n", original_message.conn_id, deserialized_message.conn_id);
  printf("o: %s d: %s\n", (char *)original_message.payload.bytes, (char *)deserialized_message.payload.bytes);
}