
#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#include "proto/message.pb.h"
#include "reactor-uc/serialization.h"

#define BUFFER_SIZE 1024
#define MSG_ID 42

void test_nanopb() {

  FederateMessage _original_msg;
  FederateMessage _deserialized_msg;

  _original_msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage* original_message = &_original_msg.message.tagged_message;
  TaggedMessage* deserialized_msg = &_deserialized_msg.message.tagged_message;
  unsigned char buffer[BUFFER_SIZE];
  unsigned char* message = NULL;
  int message_size = 0;

  original_message->conn_id = MSG_ID;
  const char* text = "Hello World1234";
  memcpy(original_message->payload.bytes, text, sizeof("Hello World1234")); // NOLINT
  original_message->payload.size = sizeof("Hello World1234");

  message = buffer;
  message_size = serialize_to_protobuf(&_original_msg, buffer, BUFFER_SIZE);
  TEST_ASSERT_TRUE(message_size > 0);

  int remaining_bytes = deserialize_from_protobuf(&_deserialized_msg, message, message_size);

  TEST_ASSERT_TRUE(remaining_bytes >= 0);

  TEST_ASSERT_EQUAL(original_message->conn_id, deserialized_msg->conn_id);
  TEST_ASSERT_EQUAL_STRING((char*)original_message->payload.bytes, (char*)deserialized_msg->payload.bytes);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_nanopb);
  return UNITY_END();
}