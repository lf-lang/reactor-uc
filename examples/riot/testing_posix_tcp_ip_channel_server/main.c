#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>

#define NUM_ITER 10

int main(void) {
  TcpIpChannel channel;

  const char *host = "127.0.0.1";
  unsigned short port = 8902; // NOLINT

  // creating a server that listens on loopback device on port 8900
  TcpIpChannel_ctor(&channel, host, port, AF_INET);

  // binding to that address
  channel.super.bind(&channel.super);

  // accept one connection
  bool new_connection;
  do {
    new_connection = channel.super.accept(&channel.super);
  } while (!new_connection);

  for (int i = 0; i < NUM_ITER; i++) {
    // waiting for messages from client
    TaggedMessage *message = channel.super.receive(&channel.super);
    printf("Received message with connection number %i and content %s\n", message->conn_id,
           (char *)message->payload.bytes);

    channel.super.send(&channel.super, message);
  }

  channel.super.close(&channel.super);
}