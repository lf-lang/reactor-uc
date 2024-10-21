#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>

#include "proto/message.pb.h"
#define NUM_ITER 10

int main() {
  TcpIpChannel channel;

  // server address
  const char *host = "127.0.0.1";
  unsigned short port = 8902; // NOLINT

  // message for server
  FederateMessage msg;
  msg.type = MessageType_TAGGED_MESSAGE;
  msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage *port_message = &msg.message.tagged_message;
  port_message->conn_id = 42; // NOLINT
  const char *message = "Hello World1234";
  memcpy(port_message->payload.bytes, message, sizeof("Hello World1234")); // NOLINT
  port_message->payload.size = sizeof("Hello World1234");

  // creating a server that listens on loopback device on port 8900
  TcpIpChannel_ctor(&channel, host, port, AF_INET);

  // binding to that address
  channel.super.connect(&channel.super);

  for (int i = 0; i < NUM_ITER; i++) {
    // sending message
    channel.super.send(&channel.super, &msg);
    // waiting for reply
    const FederateMessage *received_message = channel.super.receive(&channel.super);
    const TaggedMessage *received_tagged_message = &received_message->message.tagged_message;

    printf("Received message with connection number %i and content %s\n", received_tagged_message->conn_id,
           (char *)received_tagged_message->payload.bytes);
  }

  channel.super.close(&channel.super);
}
