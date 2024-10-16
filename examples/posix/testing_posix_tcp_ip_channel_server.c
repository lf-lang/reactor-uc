#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>

int main() {
  TcpIpChannel channel;

  const char *host = "127.0.0.1";
  unsigned short port = 8902; // NOLINT

  // creating a server that listens on loopback device on port 8900
  TcpIpChannel_ctor(&channel, host, port, AF_INET);

  // binding to that address
  channel.super.bind(&channel.super);

  // change the super to non-blocking
  channel.super.change_block_state(&channel.super, false);

  // accept one connection
  bool new_connection;
  do {
    new_connection = channel.super.accept(&channel.super);
  } while (!new_connection);

  // waiting for messages from client
  TaggedMessage *message = NULL;
  do {
    message = channel.super.receive(&channel.super);
    sleep(1);
  } while (message == NULL);

  printf("Received message with connection number %i and content %s\n", message->conn_id,
         (char *)message->payload.bytes);

  channel.super.send(&channel.super, message);

  channel.super.close(&channel.super);
}
