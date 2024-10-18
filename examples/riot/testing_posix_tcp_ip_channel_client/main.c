#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proto/message.pb.h"
#define NUM_ITER 10

int main(void) {
  TcpIpChannel channel;

  // server address
  const char *host = "127.0.0.1";
  unsigned short port = 8902; // NOLINT

  // message for server
  TaggedMessage port_message;
  port_message.conn_id = 42; // NOLINT
  const char *message = "Hello World1234";
  memcpy(port_message.payload.bytes, message, sizeof("Hello World1234")); // NOLINT
  port_message.payload.size = sizeof("Hello World1234");

  // creating a server that listens on loopback device on port 8900
  TcpIpChannel_ctor(&channel, host, port, AF_INET);

  // binding to that address
  channel.super.connect(&channel.super);

  for (int i = 0; i < NUM_ITER; i++) {
    // sending message
    channel.super.send(&channel.super, &port_message);

    // waiting for reply
    TaggedMessage *received_message = channel.super.receive(&channel.super);

    printf("Received message with connection number %" PRIi32 " and content %s\n", received_message->conn_id,
           (char *)received_message->payload.bytes);
  }

  channel.super.close(&channel.super);
}