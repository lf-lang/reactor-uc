#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "unity.h"
#include <inttypes.h>
#include <sys/socket.h>
#include <unistd.h>

#define MESSAGE_CONTENT "Hello World1234"
#define MESSAGE_CONNECTION_ID 42

NetworkChannel *server_channel;
NetworkChannel *client_channel;
TcpIpChannel _server_tcp_channel;
TcpIpChannel _client_tcp_channel;

bool server_callback_called = false;
bool client_callback_called = false;

void test_init_tcp_ip(void) {
  const char *host = "127.0.0.1";
  unsigned short port = 8903; // NOLINT

  /* init server */
  TcpIpChannel_ctor(&_server_tcp_channel, host, port, AF_INET, true);
  server_channel = &_server_tcp_channel.super;

  /* init client */
  TcpIpChannel_ctor(&_client_tcp_channel, host, port, AF_INET, false);
  client_channel = &_client_tcp_channel.super;
}

void test_connect(void) {
  // open connection
  server_channel->open_connection(server_channel);
  client_channel->open_connection(client_channel);

  // try connect
  bool client_connected = false;
  bool server_connected = false;
  while (!client_connected && !server_connected) {
    /* client tries to connect to server */
    if (client_channel->try_connect(client_channel) == LF_OK) {
      client_connected = true;
    }

    /* server tries to connect to client */
    if (server_channel->try_connect(server_channel) == LF_OK) {
      server_connected = true;
    }
  }
}

void server_callback_handler(FederatedConnectionBundle *self, TaggedMessage *msg) {
  (void)self;
  printf("\nServer: Received message with connection number %i and content %s\n", msg->conn_id,
         (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

  server_callback_called = true;
}

void test_client_send_and_server_recv(void) {
  // register receive callback for handling incoming messages
  server_channel->register_receive_callback(server_channel, server_callback_handler, NULL);

  /* create message */
  TaggedMessage port_message;
  port_message.conn_id = MESSAGE_CONNECTION_ID;
  const char *message = MESSAGE_CONTENT;
  memcpy(port_message.payload.bytes, message, sizeof(MESSAGE_CONTENT));
  port_message.payload.size = sizeof(MESSAGE_CONTENT);

  /* send message */
  client_channel->send_blocking(client_channel, &port_message);

  /* wait for the callback */
  sleep(1);

  /* check if the callback was called */
  TEST_ASSERT_TRUE(server_callback_called);
}

void client_callback_handler(FederatedConnectionBundle *self, TaggedMessage *msg) {
  (void)self;
  printf("\nClient: Received message with connection number %i and content %s\n", msg->conn_id,
         (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char *)msg->payload.bytes);
  TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

  client_callback_called = true;
}

void test_server_send_and_client_recv(void) {
  // register receive callback for handling incoming messages
  client_channel->register_receive_callback(client_channel, client_callback_handler, NULL);

  /* create message */
  TaggedMessage port_message;
  port_message.conn_id = MESSAGE_CONNECTION_ID;
  const char *message = MESSAGE_CONTENT;
  memcpy(port_message.payload.bytes, message, sizeof(MESSAGE_CONTENT));
  port_message.payload.size = sizeof(MESSAGE_CONTENT);

  /* send message */
  server_channel->send_blocking(server_channel, &port_message);

  /* wait for the callback */
  sleep(1);

  /* check if the callback was called */
  TEST_ASSERT_TRUE(client_callback_called);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_init_tcp_ip);
  RUN_TEST(test_connect);
  RUN_TEST(test_client_send_and_server_recv);
  RUN_TEST(test_server_send_and_client_recv);
  return UNITY_END();
}
