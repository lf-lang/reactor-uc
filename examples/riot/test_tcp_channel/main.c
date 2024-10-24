#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>
#include <sys/socket.h>

NetworkChannel *server_channel;
NetworkChannel *client_channel;

void callback_handler(FederatedConnectionBundle *self, TaggedMessage *msg) {
  (void)self;
  printf("Received message with connection number %i and content %s\n", msg->conn_id, (char *)msg->payload.bytes);
  // channel.super.send_blocking(&channel.super, msg);
}

NetworkChannel *server_init(TcpIpChannel *tcp_channel, const char *host, unsigned short port) {
  NetworkChannel *channel = &tcp_channel->super;

  // creating a server that listens on loopback device on port 8903
  TcpIpChannel_ctor(tcp_channel, host, port, AF_INET, true);

  // binding to that address
  channel->open_connection(channel);

  // register receive callback for handling incoming messages
  channel->register_receive_callback(channel, callback_handler, NULL);

  return channel;
}

NetworkChannel *client_init(TcpIpChannel *tcp_channel, const char *host, unsigned short port) {
  NetworkChannel *channel = &tcp_channel->super;

  // creating a server that listens on loopback device on port 8900
  TcpIpChannel_ctor(tcp_channel, host, port, AF_INET, false);

  // binding to that address
  channel->open_connection(channel);

  return channel;
}

int main() {
  const char *host = "127.0.0.1";
  unsigned short port = 8903; // NOLINT

  /* init server and wait for messages */
  TcpIpChannel _server_tcp_channel;
  server_channel = server_init(&_server_tcp_channel, host, port);

  /* init client and send messages to server */
  TcpIpChannel _client_tcp_channel;
  client_channel = client_init(&_client_tcp_channel, host, port);

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

  /* send data to server */
  TaggedMessage port_message;
  port_message.conn_id = 42; // NOLINT
  const char *message = "Hello World1234";
  memcpy(port_message.payload.bytes, message, sizeof("Hello World1234")); // NOLINT
  port_message.payload.size = sizeof("Hello World1234");
  for (int i = 0; i < 10; i++) {
    client_channel->send_blocking(client_channel, &port_message);

    // waiting for reply
    // TaggedMessage *received_message = channel->receive(channel);

    // printf("Received message with connection number %i and content %s\n", received_message->conn_id, (char
    // *)received_message->payload.bytes);
  }
}
