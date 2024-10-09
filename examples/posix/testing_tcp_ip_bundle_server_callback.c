#include "reactor-uc/federated.h"
#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>
TcpIpBundle bundle;

void callback_handler(FederatedConnectionBundle *self, PortMessage *msg) {
  printf("Received message with connection number %i and content %s\n", msg->connection_number, msg->message);

  bundle.send(&bundle, msg);
}

int main() {

  const char *host = "127.0.0.1";
  unsigned short port = 8900; // NOLINT

  // creating a server that listens on loopback device on port 8900
  TcpIpBundle_ctor(&bundle, host, port, AF_INET);

  // binding to that address
  bundle.bind(&bundle);

  // change the bundle to non-blocking
  bundle.change_block_state(&bundle, false);

  // accept one connection
  bool new_connection;
  do {
    new_connection = bundle.accept(&bundle);
  } while (!new_connection);

  bundle.register_callback(&bundle, callback_handler, NULL);

  sleep(100);

  bundle.close(&bundle);
}
