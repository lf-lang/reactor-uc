#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "unity.h"
#include "../../../unit/test_util.h"
#include <inttypes.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ztimer.h"

#define MESSAGE_CONTENT "Hello World1234"
#define MESSAGE_CONNECTION_ID 42
#define REMOTE_ADDRESS "::1"
#define REMOTE_PROTOCOL_FAMILY AF_INET6

Reactor parent;
FederateEnvironment env;
Environment *_lf_environment = &env.super;
FederatedConnectionBundle bundle;
FederatedConnectionBundle *net_bundles[] = {&bundle};
StartupCoordinator startup_coordinator;

CoapUdpIpChannel _coap_channel;
NetworkChannel *channel = &_coap_channel.super;

bool server_callback_called = false;
bool client_callback_called = false;

void setUp(void) {
  /* init environment */
  FederateEnvironment_ctor(&env, NULL, NULL, false, net_bundles, 1, &startup_coordinator, NULL);

  /* init channel */
  CoapUdpIpChannel_ctor(&_coap_channel, REMOTE_ADDRESS, REMOTE_PROTOCOL_FAMILY);

  /* init bundle */
  FederatedConnectionBundle_ctor(&bundle, &parent, channel, NULL, NULL, 0, NULL, NULL, 0, 0);
}

void tearDown(void) { channel->free(channel); }

/* TESTS */
void test_open_connection_non_blocking(void) {
  TEST_ASSERT_OK(channel->open_connection(channel));

  ztimer_sleep(ZTIMER_SEC, 1);

  TEST_ASSERT_TRUE(channel->is_connected(channel));
}

void server_callback_handler(FederatedConnectionBundle *self, const FederateMessage *_msg) {
  (void)self;
  const TaggedMessage *msg = &_msg->message.tagged_message;
  printf("\nServer: Received message with connection number %" PRId32 " and content %s\n", msg->conn_id,
         (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

  server_callback_called = true;
}

void test_client_send_and_server_recv(void) {
  // open connection
  TEST_ASSERT_OK(channel->open_connection(channel));

  // Wait until channel is connected
  while (!channel->is_connected(channel)) {
    ztimer_sleep(ZTIMER_SEC, 1);
  }

  // register receive callback for handling incoming messages
  channel->register_receive_callback(channel, server_callback_handler, NULL);

  /* create message */
  FederateMessage msg;
  msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage *port_message = &msg.message.tagged_message;
  port_message->conn_id = MESSAGE_CONNECTION_ID;
  const char *message = MESSAGE_CONTENT;
  memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
  port_message->payload.size = sizeof(MESSAGE_CONTENT);

  /* send message */
  TEST_ASSERT_OK(channel->send_blocking(channel, &msg));

  /* wait for the callback */
  ztimer_sleep(ZTIMER_SEC, 1);

  /* check if the callback was called */
  TEST_ASSERT_TRUE(server_callback_called);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_open_connection_non_blocking);
  RUN_TEST(test_client_send_and_server_recv);
  exit(UNITY_END());
}
