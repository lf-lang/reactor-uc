#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>

#include "proto/message.pb.h"

#define PORT_NUM 8900
#define IP_ADDR "127.0.0.1"
#define CONN_ID 42

int main() {
  TcpIpBundle bundle;

  // server address
  const char *host = IP_ADDR;
  unsigned short port = PORT_NUM;

  // message for server
  PortMessage port_message;
  port_message.connection_number = CONN_ID;
  const char *message = "Hello World1234";
  memcpy(port_message.message, message, sizeof("Hello World1234"));

  // creating a server that listens on loopback device on port 8900
  TcpIpBundle_ctor(&bundle, host, port, AF_INET);

  // binding to that address
  bundle.connect(&bundle);

  // change the bundle to non-blocking
  bundle.change_block_state(&bundle, false);

  // sending message
  bundle.send(&bundle, &port_message);

  // waiting for reply
  PortMessage *received_message = NULL;
  do {
    received_message = bundle.receive(&bundle);
  } while (received_message == NULL);

  printf("Received message with connection number %i and content %s\n", received_message->connection_number,
         received_message->message);

  bundle.close(&bundle);
}
