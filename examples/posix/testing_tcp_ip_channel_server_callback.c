#include "reactor-uc/federated.h"
#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>
TcpIpChannel channel;

void callback_handler(FederatedConnectionBundle *self, TaggedMessage *msg) {
  printf("Received message with connection number %i and content %s\n", msg->conn_id, (char *)msg->payload.bytes);
  channel.super.send(&channel, msg);
}

int main() {

  const char *host = "127.0.0.1";
  unsigned short port = 8903; // NOLINT

  // creating a server that listens on loopback device on port 8900
  TcpIpChannel_ctor(&channel, host, port, AF_INET);

  // binding to that address
  channel.super.bind(&channel);

  // change the super to non-blocking
  channel.super.change_block_state(&channel, false);

  // accept one connection
  bool new_connection;
  do {
    new_connection = channel.super.accept(&channel);
  } while (!new_connection);

  channel.super.register_callback(&channel, callback_handler, NULL);

  sleep(100);

  channel.super.close(&channel);
}
