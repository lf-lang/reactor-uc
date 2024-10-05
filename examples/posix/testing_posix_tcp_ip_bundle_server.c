#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>

#define PORT_NUM 8900
#define IP_ADDR "127.0.0.1"

int main() {
  TcpIpBundle bundle;

  const char *host = IP_ADDR;
  unsigned short port = PORT_NUM;

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

  // waiting for messages from client
  PortMessage *message = NULL;
  do {
    message = bundle.receive(&bundle);
    sleep(1);
  } while (message == NULL);

  printf("Received message with connection number %i and content %s\n", message->connection_number, message->message);

  bundle.send(&bundle, message);

  bundle.close(&bundle);
}
