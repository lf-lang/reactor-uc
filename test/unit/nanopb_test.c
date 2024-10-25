
#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#include "proto/message.pb.h"
#include "reactor-uc/encoding.h"

#define BUFFER_SIZE 1024
#define MSG_ID 42

void test_nanopb() {

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
  TEST_ASSERT_TRUE(message_size > 0);

  int remaining_bytes = decode_protobuf(&deserialized_message, message, message_size);

  TEST_ASSERT_TRUE(remaining_bytes >= 0);

  TEST_ASSERT_EQUAL(original_message.conn_id, deserialized_message.conn_id);
  TEST_ASSERT_EQUAL_STRING((char *)original_message.payload.bytes, (char *)deserialized_message.payload.bytes);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_nanopb);
  return UNITY_END();
}