/**
 * Platform test of the Zephyr CoAP network channel (see test/platform/riot/coap_channel_test for the RIOT version).
 *
 * The channel connects to the CoAP server of its own federate over the loopback interface, i.e. the same
 * channel is client and server: /connect and /message requests sent by the channel are handled by the
 * server handlers of the very same channel.
 */
#include "reactor-uc/platform/zephyr/coap_udp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "unity.h"
#include "../../../../unit/test_util.h"
#include <inttypes.h>
#include <string.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_ARCH_POSIX
#include "posix_board_if.h"
#endif

#define MESSAGE_CONTENT "Hello World1234"
#define MESSAGE_CONNECTION_ID 42
#define REMOTE_ADDRESS "127.0.0.1"
#define REMOTE_PROTOCOL_FAMILY AF_INET
#define CONNECT_TIMEOUT_MS 2000
#define NUM_MESSAGES 10

Reactor parent;
FederatedEnvironment env;
Environment* _lf_environment = &env.super;
FederatedConnectionBundle bundle;
FederatedConnectionBundle* net_bundles[] = {&bundle};
StartupCoordinator startup_coordinator;
ShutdownCoordinator shutdown_coordinator;

CoapUdpIpChannel _coap_channel;
NetworkChannel* channel = &_coap_channel.super;

/* Filled by the receive callback, which runs in the Zephyr CoAP server thread. The assertions are done in the
 * test thread (Unity must not fail from another thread). */
static int received_count = 0;
static int32_t received_conn_ids[NUM_MESSAGES];
static char received_payload[sizeof(MESSAGE_CONTENT)];
static FederatedConnectionBundle* received_bundle = NULL;

void setUp(void) {
  /* init environment */
  FederatedEnvironment_ctor(&env, NULL, NULL, false, net_bundles, 1, &startup_coordinator, &shutdown_coordinator, NULL);

  /* init channel */
  CoapUdpIpChannel_ctor(&_coap_channel, REMOTE_ADDRESS, REMOTE_PROTOCOL_FAMILY);

  /* init bundle */
  FederatedConnectionBundle_ctor(&bundle, &parent, channel, NULL, NULL, 0, NULL, NULL, 0, 0);

  received_count = 0;
  received_bundle = NULL;
  memset(received_conn_ids, 0, sizeof(received_conn_ids));
  memset(received_payload, 0, sizeof(received_payload));
}

void tearDown(void) {
  channel->free(channel);
  /* Let outstanding requests (e.g. /disconnect) of this test complete, so they do not reach the channel of the
   * next test, which has the same remote address. Zephyr's CoAP client thread only picks up a client that is not
   * yet polled after its periodic poll timeout (500 ms), so a response can take that long. */
  k_msleep(600);
}

/* HELPERS */
static bool wait_until_connected(void) {
  for (int i = 0; i < CONNECT_TIMEOUT_MS / 10; i++) {
    if (channel->is_connected(channel)) {
      return true;
    }
    k_msleep(10);
  }
  return channel->is_connected(channel);
}

static void build_message(FederateMessage* msg, int32_t conn_id) {
  memset(msg, 0, sizeof(*msg));
  msg->which_message = FederateMessage_tagged_message_tag;

  TaggedMessage* port_message = &msg->message.tagged_message;
  port_message->conn_id = conn_id;
  memcpy(port_message->payload.bytes, MESSAGE_CONTENT, sizeof(MESSAGE_CONTENT)); // NOLINT
  port_message->payload.size = sizeof(MESSAGE_CONTENT);
}

void server_callback_handler(FederatedConnectionBundle* self, const FederateMessage* _msg) {
  const TaggedMessage* msg = &_msg->message.tagged_message;
  printf("\nServer: Received message with connection number %" PRId32 " and content %s\n", msg->conn_id,
         (char*)msg->payload.bytes);

  if (received_count < NUM_MESSAGES) {
    received_conn_ids[received_count] = msg->conn_id;
  }
  memcpy(received_payload, msg->payload.bytes, sizeof(received_payload));
  received_payload[sizeof(received_payload) - 1] = '\0';
  received_bundle = self;
  received_count++;
}

/* TESTS */
void test_open_connection_non_blocking(void) {
  TEST_ASSERT_OK(channel->open_connection(channel));

  TEST_ASSERT_TRUE(wait_until_connected());
}

void test_send_fails_if_not_connected(void) {
  FederateMessage msg;
  build_message(&msg, MESSAGE_CONNECTION_ID);

  TEST_ASSERT_FALSE(channel->is_connected(channel));
  TEST_ASSERT_EQUAL(LF_ERR, channel->send_blocking(channel, &msg));
  TEST_ASSERT_EQUAL(0, received_count);
}

void test_client_send_and_server_recv(void) {
  // open connection
  TEST_ASSERT_OK(channel->open_connection(channel));

  // Wait until channel is connected
  TEST_ASSERT_TRUE(wait_until_connected());

  // register receive callback for handling incoming messages
  channel->register_receive_callback(channel, server_callback_handler, &bundle);

  /* create message */
  FederateMessage msg;
  build_message(&msg, MESSAGE_CONNECTION_ID);

  /* send message (returns after the message is acknowledged by the server) */
  TEST_ASSERT_OK(channel->send_blocking(channel, &msg));

  /* wait for the callback */
  for (int i = 0; i < 100 && received_count == 0; i++) {
    k_msleep(10);
  }

  /* check if the callback was called with the message */
  TEST_ASSERT_EQUAL(1, received_count);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, received_conn_ids[0]);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, received_payload);
  TEST_ASSERT_EQUAL_PTR(&bundle, received_bundle);
}

void test_send_multiple_messages_in_order(void) {
  TEST_ASSERT_OK(channel->open_connection(channel));
  TEST_ASSERT_TRUE(wait_until_connected());
  channel->register_receive_callback(channel, server_callback_handler, &bundle);

  FederateMessage msg;
  for (int32_t i = 0; i < NUM_MESSAGES; i++) {
    build_message(&msg, i);
    TEST_ASSERT_OK(channel->send_blocking(channel, &msg));
  }

  for (int i = 0; i < 100 && received_count < NUM_MESSAGES; i++) {
    k_msleep(10);
  }

  // Every message is delivered exactly once and in order.
  TEST_ASSERT_EQUAL(NUM_MESSAGES, received_count);
  for (int32_t i = 0; i < NUM_MESSAGES; i++) {
    TEST_ASSERT_EQUAL(i, received_conn_ids[i]);
  }
}

void test_close_connection(void) {
  TEST_ASSERT_OK(channel->open_connection(channel));
  TEST_ASSERT_TRUE(wait_until_connected());
  channel->register_receive_callback(channel, server_callback_handler, &bundle);

  channel->close_connection(channel);
  TEST_ASSERT_FALSE(channel->is_connected(channel));

  // Sending on a closed channel fails and nothing is delivered.
  FederateMessage msg;
  build_message(&msg, MESSAGE_CONNECTION_ID);
  TEST_ASSERT_EQUAL(LF_ERR, channel->send_blocking(channel, &msg));
  k_msleep(100);
  TEST_ASSERT_EQUAL(0, received_count);
}

void test_reopen_connection_after_close(void) {
  TEST_ASSERT_OK(channel->open_connection(channel));
  TEST_ASSERT_TRUE(wait_until_connected());
  channel->close_connection(channel);
  TEST_ASSERT_FALSE(channel->is_connected(channel));

  // Let the /disconnect request complete before reconnecting.
  k_msleep(100);

  TEST_ASSERT_OK(channel->open_connection(channel));
  TEST_ASSERT_TRUE(wait_until_connected());

  channel->register_receive_callback(channel, server_callback_handler, &bundle);
  FederateMessage msg;
  build_message(&msg, MESSAGE_CONNECTION_ID);
  TEST_ASSERT_OK(channel->send_blocking(channel, &msg));
  for (int i = 0; i < 100 && received_count == 0; i++) {
    k_msleep(10);
  }
  TEST_ASSERT_EQUAL(1, received_count);
}

void test_connect_after_free_with_request_in_flight(void) {
  TEST_ASSERT_OK(channel->open_connection(channel));
  TEST_ASSERT_TRUE(wait_until_connected());

  // Keep Zephyr's CoAP client thread polling the data client: a client that is not yet polled (here: the
  // control client) is only picked up after the poll timeout of 500 ms, which delays its responses.
  channel->register_receive_callback(channel, server_callback_handler, &bundle);
  FederateMessage msg;
  build_message(&msg, MESSAGE_CONNECTION_ID);
  TEST_ASSERT_OK(channel->send_blocking(channel, &msg));
  k_msleep(600);

  // close_connection returns as soon as the /disconnect request is sent. The server handles it right away,
  // but the response arrives with the delay described above.
  channel->close_connection(channel);

  // Free the channel while its /disconnect request (on the only control client) is still in flight and
  // immediately open a new channel. Its connect attempt finds no free control client and must be retried once
  // the /disconnect response releases the client, although that response has no owner anymore.
  channel->free(channel);
  CoapUdpIpChannel_ctor(&_coap_channel, REMOTE_ADDRESS, REMOTE_PROTOCOL_FAMILY);
  TEST_ASSERT_OK(channel->open_connection(channel));
  TEST_ASSERT_TRUE(wait_until_connected());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_open_connection_non_blocking);
  RUN_TEST(test_send_fails_if_not_connected);
  RUN_TEST(test_client_send_and_server_recv);
  RUN_TEST(test_send_multiple_messages_in_order);
  RUN_TEST(test_close_connection);
  RUN_TEST(test_reopen_connection_after_close);
  RUN_TEST(test_connect_after_free_with_request_in_flight);
  int failures = UNITY_END();
#ifdef CONFIG_ARCH_POSIX
  posix_exit(failures);
#endif
  return failures;
}
