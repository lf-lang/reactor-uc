#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/network_bundles.h"
#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
/*
struct timeval timeout;
timeout.tv_sec = 0;
timeout.tv_usec = 100; //100 us (microseconds)

fd_set set;
FD_ZERO(&set);
FD_SET(bundle.fd, &set);

int retval;

do {
retval = select(1, NULL, &set, NULL, &timeout);
} while (retval == 0);
 */

int main() {
  TcpIpBundle bundle;

  const char* host = "127.0.0.1";
  unsigned short port = 8900;

  // creating a server that listens on loopback device on port 8900
  TcpIpBundle_ctor(&bundle, host, port, AF_INET);

  // binding to that address
  bundle.bind(&bundle);

  // accept one connection
  bool new_connection;
  do {
     new_connection = bundle.accept(&bundle);
  } while (!new_connection);

  PortMessage* message = NULL;
  sleep(1);

  do {
    message = bundle.receive(&bundle);
  } while (message == NULL);

  printf("Received message with connection number %i\n", message->connection_number);

  bundle.send(&bundle, message);

  bundle.close(&bundle);
}
