#include "reactor-uc/network_channel/zenoh_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/startup_coordinator.h"
#include "unity.h"
#include "test_util.h"
#include <stdlib.h>
#include <unistd.h>

/*
 * Two federates in one process, each with a Zenoh session of its own.
 *
 * By default the two sessions are peers that connect to each other directly,
 * so the test needs no Zenoh router. With ZENOH_CHANNEL_TEST_ROUTER set to the
 * locator of a router, e.g. tcp/127.0.0.1:7447, both sessions are clients of
 * that router and the messages are relayed.
 */

#define MESSAGE_CONTENT "Hello World1234"
#define MESSAGE_CONNECTION_ID 42
#define PEER_LOCATOR "tcp/127.0.0.1:17447"
#define KEY_PREFIX "lf/zenoh_channel_test"

Reactor parent;
FederatedEnvironment env;
Environment* _lf_environment = &env.super;
FederatedConnectionBundle left_bundle;
FederatedConnectionBundle right_bundle;
FederatedConnectionBundle* net_bundles[] = {&left_bundle, &right_bundle};
StartupCoordinator startup_coordinator;
ShutdownCoordinator shutdown_coordinator;

ZenohChannelSession left_session;
ZenohChannelSession right_session;
ZenohChannel _left_zenoh_channel;
ZenohChannel _right_zenoh_channel;
NetworkChannel* left_channel = &_left_zenoh_channel.super;
NetworkChannel* right_channel = &_right_zenoh_channel.super;

bool left_callback_called = false;
bool right_callback_called = false;

void setUp(void) {
  const char* router = getenv("ZENOH_CHANNEL_TEST_ROUTER");

  /* init environment */
  FederatedEnvironment_ctor(&env, NULL, NULL, false, net_bundles, 2, &startup_coordinator, &shutdown_coordinator, NULL);

  /* init sessions */
  if (router != NULL) {
    TEST_ASSERT_OK(ZenohChannelSession_open(&left_session, "client", router, NULL));
    TEST_ASSERT_OK(ZenohChannelSession_open(&right_session, "client", router, NULL));
  } else {
    TEST_ASSERT_OK(ZenohChannelSession_open(&left_session, "peer", NULL, PEER_LOCATOR));
    TEST_ASSERT_OK(ZenohChannelSession_open(&right_session, "peer", PEER_LOCATOR, NULL));
  }

  /* init channels: the same two names, swapped */
  ZenohChannel_ctor(&_left_zenoh_channel, &left_session, KEY_PREFIX, "left", "right");
  ZenohChannel_ctor(&_right_zenoh_channel, &right_session, KEY_PREFIX, "right", "left");

  /* init bundles */
  FederatedConnectionBundle_ctor(&left_bundle, &parent, left_channel, NULL, NULL, 0, NULL, NULL, 0, 0);
  FederatedConnectionBundle_ctor(&right_bundle, &parent, right_channel, NULL, NULL, 0, NULL, NULL, 0, 0);

  left_callback_called = false;
  right_callback_called = false;
}

void tearDown(void) {
  left_channel->free(left_channel);
  right_channel->free(right_channel);
  ZenohChannelSession_close(&right_session);
  ZenohChannelSession_close(&left_session);
}

static void open_and_wait_until_connected(void) {
  TEST_ASSERT_OK(left_channel->open_connection(left_channel));
  TEST_ASSERT_OK(right_channel->open_connection(right_channel));

  for (int i = 0; i < 50; i++) {
    if (left_channel->is_connected(left_channel) && right_channel->is_connected(right_channel)) {
      return;
    }
    usleep(100000);
  }
  TEST_FAIL_MESSAGE("The channels did not connect within 5 s");
}

static void fill_message(FederateMessage* msg) {
  msg->which_message = FederateMessage_tagged_message_tag;

  TaggedMessage* port_message = &msg->message.tagged_message;
  port_message->conn_id = MESSAGE_CONNECTION_ID;
  const char* message = MESSAGE_CONTENT;
  memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
  port_message->payload.size = sizeof(MESSAGE_CONTENT);
}

/* TESTS */
void test_not_connected_before_open(void) {
  TEST_ASSERT_FALSE(left_channel->is_connected(left_channel));
  TEST_ASSERT_FALSE(right_channel->is_connected(right_channel));
}

void test_not_connected_while_the_other_side_is_missing(void) {
  FederateMessage msg;
  fill_message(&msg);

  TEST_ASSERT_OK(left_channel->open_connection(left_channel));
  sleep(1);

  TEST_ASSERT_FALSE(left_channel->is_connected(left_channel));
  TEST_ASSERT_EQUAL(LF_ERR, left_channel->send_blocking(left_channel, &msg));
}

void test_open_connection_non_blocking(void) { open_and_wait_until_connected(); }

void left_callback_handler(FederatedConnectionBundle* self, const FederateMessage* _msg) {
  (void)self;
  const TaggedMessage* msg = &_msg->message.tagged_message;
  printf("\nLeft: Received message with connection number %i and content %s\n", msg->conn_id,
         (char*)msg->payload.bytes);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char*)msg->payload.bytes);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

  left_callback_called = true;
}

void right_callback_handler(FederatedConnectionBundle* self, const FederateMessage* _msg) {
  (void)self;
  const TaggedMessage* msg = &_msg->message.tagged_message;
  printf("\nRight: Received message with connection number %i and content %s\n", msg->conn_id,
         (char*)msg->payload.bytes);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char*)msg->payload.bytes);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

  right_callback_called = true;
}

void test_left_send_and_right_recv(void) {
  FederateMessage msg;
  fill_message(&msg);

  open_and_wait_until_connected();
  left_channel->register_receive_callback(left_channel, left_callback_handler, NULL);
  right_channel->register_receive_callback(right_channel, right_callback_handler, NULL);

  TEST_ASSERT_OK(left_channel->send_blocking(left_channel, &msg));

  /* wait for the callback */
  sleep(1);

  TEST_ASSERT_TRUE(right_callback_called);
  /* a federate does not receive what it sent */
  TEST_ASSERT_FALSE(left_callback_called);
}

void test_right_send_and_left_recv(void) {
  FederateMessage msg;
  fill_message(&msg);

  open_and_wait_until_connected();
  left_channel->register_receive_callback(left_channel, left_callback_handler, NULL);
  right_channel->register_receive_callback(right_channel, right_callback_handler, NULL);

  TEST_ASSERT_OK(right_channel->send_blocking(right_channel, &msg));

  /* wait for the callback */
  sleep(1);

  TEST_ASSERT_TRUE(left_callback_called);
  TEST_ASSERT_FALSE(right_callback_called);
}

void test_not_connected_after_the_other_side_closed(void) {
  open_and_wait_until_connected();

  right_channel->close_connection(right_channel);
  sleep(1);

  TEST_ASSERT_FALSE(left_channel->is_connected(left_channel));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_not_connected_before_open);
  RUN_TEST(test_not_connected_while_the_other_side_is_missing);
  RUN_TEST(test_open_connection_non_blocking);
  RUN_TEST(test_left_send_and_right_recv);
  RUN_TEST(test_right_send_and_left_recv);
  RUN_TEST(test_not_connected_after_the_other_side_closed);
  return UNITY_END();
}
