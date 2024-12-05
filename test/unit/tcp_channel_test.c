#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "unity.h"
#include "test_util.h"
#include <inttypes.h>
#include <sys/socket.h>
#include <unistd.h>

#define MESSAGE_CONTENT "Hello World1234"
#define MESSAGE_CONNECTION_ID 42
#define HOST "127.0.0.1"
#define PORT 8903

Reactor parent;
Environment env;
FederatedConnectionBundle server_bundle;
FederatedConnectionBundle client_bundle;
FederatedConnectionBundle *net_bundles[] = {&server_bundle, &client_bundle};

TcpIpChannel _server_tcp_channel;
TcpIpChannel _client_tcp_channel;
NetworkChannel *server_channel = &_server_tcp_channel.super;
NetworkChannel *client_channel = &_client_tcp_channel.super;

bool server_callback_called = false;
bool client_callback_called = false;

void setUp(void) {
  /* init environment */
  Environment_ctor(&env, NULL);
  env.net_bundles = net_bundles;
  env.net_bundles_size = 2;

  /* init server */
  TcpIpChannel_ctor(&_server_tcp_channel, &env, HOST, PORT, AF_INET, true);

  /* init client */
  TcpIpChannel_ctor(&_client_tcp_channel, &env, HOST, PORT, AF_INET, false);

  /* init bundles */
  FederatedConnectionBundle_ctor(&server_bundle, &parent, server_channel, NULL, NULL, 0, NULL, NULL, 0);
  FederatedConnectionBundle_ctor(&client_bundle, &parent, client_channel, NULL, NULL, 0, NULL, NULL, 0);
}

void tearDown(void) {
  server_channel->free(server_channel);
  client_channel->free(client_channel);
}

/* TESTS */
void test_open_connection_non_blocking(void) {
  TEST_ASSERT_OK(server_channel->open_connection(server_channel));
  TEST_ASSERT_OK(client_channel->open_connection(client_channel));

  sleep(1);

  TEST_ASSERT_EQUAL(NETWORK_CHANNEL_STATE_CONNECTED, server_channel->get_connection_state(server_channel));
  TEST_ASSERT_EQUAL(NETWORK_CHANNEL_STATE_CONNECTED, client_channel->get_connection_state(client_channel));
}

void server_callback_handler(FederatedConnectionBundle *self, const FederateMessage *_msg) {
  (void)self;
  const TaggedMessage *msg = &_msg->message.tagged_message;
  printf("\nServer: Received message with connection number %i and content %s\n", msg->conn_id,
         (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

  server_callback_called = true;
}

void test_client_send_and_server_recv(void) {
  // open connection
  TEST_ASSERT_OK(server_channel->open_connection(server_channel));
  TEST_ASSERT_OK(client_channel->open_connection(client_channel));

  // wait for connection to be established
  while (server_channel->get_connection_state(server_channel) != NETWORK_CHANNEL_STATE_CONNECTED &&
         client_channel->get_connection_state(client_channel) != NETWORK_CHANNEL_STATE_CONNECTED) {
    sleep(1);
  }

  // register receive callback for handling incoming messages
  server_channel->register_receive_callback(server_channel, server_callback_handler, NULL);

  /* create message */
  FederateMessage msg;
  msg.type = MessageType_TAGGED_MESSAGE;
  msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage *port_message = &msg.message.tagged_message;
  port_message->conn_id = MESSAGE_CONNECTION_ID;
  const char *message = MESSAGE_CONTENT;
  memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
  port_message->payload.size = sizeof(MESSAGE_CONTENT);

  /* send message */
  TEST_ASSERT_OK(client_channel->send_blocking(client_channel, &msg));

  /* wait for the callback */
  sleep(1);

  /* check if the callback was called */
  TEST_ASSERT_TRUE(server_callback_called);
}

void client_callback_handler(FederatedConnectionBundle *self, const FederateMessage *_msg) {
  (void)self;
  const TaggedMessage *msg = &_msg->message.tagged_message;
  printf("\nClient: Received message with connection number %i and content %s\n", msg->conn_id,
         (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

  client_callback_called = true;
}

void test_server_send_and_client_recv(void) {
  // open connection
  TEST_ASSERT_OK(server_channel->open_connection(server_channel));
  TEST_ASSERT_OK(client_channel->open_connection(client_channel));

  // wait for connection to be established
  while (server_channel->get_connection_state(server_channel) != NETWORK_CHANNEL_STATE_CONNECTED &&
         client_channel->get_connection_state(client_channel) != NETWORK_CHANNEL_STATE_CONNECTED) {
    sleep(1);
  }

  // register receive callback for handling incoming messages
  client_channel->register_receive_callback(client_channel, client_callback_handler, NULL);

  /* create message */
  FederateMessage msg;
  msg.type = MessageType_TAGGED_MESSAGE;
  msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage *port_message = &msg.message.tagged_message;
  port_message->conn_id = MESSAGE_CONNECTION_ID;
  const char *message = MESSAGE_CONTENT;
  memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
  port_message->payload.size = sizeof(MESSAGE_CONTENT);

  /* send message */
  TEST_ASSERT_OK(server_channel->send_blocking(server_channel, &msg));

  /* wait for the callback */
  sleep(1);

  /* check if the callback was called */
  TEST_ASSERT_TRUE(client_callback_called);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_open_connection_non_blocking);
  RUN_TEST(test_client_send_and_server_recv);
  RUN_TEST(test_server_send_and_client_recv);
  return UNITY_END();
}
