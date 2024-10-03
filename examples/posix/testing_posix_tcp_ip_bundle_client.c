#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/network_bundles.h"
#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  TcpIpBundle bundle;

  const char* host = "127.0.0.1";
  unsigned short port = 8900;

  // creating a server that listens on loopback device on port 8900
  TcpIpBundle_Server_Ctor(&bundle, host, port, AF_INET);

  // binding to that address
  bundle.connect(&bundle);

  PortMessage port_message;
  port_message.connection_number = 42;
  const char* message = "Hello World1234";
  memcpy(port_message.message, message, sizeof(*message));


  PortMessage* received_message = NULL;

  sleep(2);

  fflush(stdout);
  bundle.send(&bundle, &port_message);


    do {
      received_message = bundle.receive(&bundle);
    } while (received_message == NULL);

  printf("Received message with connection number %i\n", received_message->connection_number);

  bundle.close(&bundle);
}
