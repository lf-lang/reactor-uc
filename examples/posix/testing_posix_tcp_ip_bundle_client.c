#include "reactor-uc/platform/posix/tcp_ip_bundle.h"
#include "reactor-uc/reactor-uc.h"
#include <sys/socket.h>
#include <unistd.h>

#include "proto/message.pb.h"
#define NUM_ITER 10

int main() {
  TcpIpBundle bundle;

  // server address
  const char *host = "127.0.0.1";
  unsigned short port = 8900; // NOLINT

  // message for server
  TaggedMessage port_message;
  port_message.conn_id = 42; // NOLINT
  const char *message = "Hello World1234";
  memcpy(port_message.payload.bytes, message, sizeof("Hello World1234")); // NOLINT
  port_message.payload.size = sizeof("Hello World1234");

  // creating a server that listens on loopback device on port 8900
  TcpIpBundle_ctor(&bundle, host, port, AF_INET);

  // binding to that address
  bundle.connect(&bundle);

  // change the bundle to non-blocking
  bundle.change_block_state(&bundle, false);

  for (int i = 0; i < NUM_ITER; i++) {
    // sending message
    bundle.send(&bundle, &port_message);

    // waiting for reply
    TaggedMessage *received_message = NULL;
    do {
      received_message = bundle.receive(&bundle);
    } while (received_message == NULL);

    printf("Received message with connection number %i and content %s\n", received_message->conn_id,
           (char *)received_message->payload.bytes);

    sleep(i);
  }

  bundle.close(&bundle);
}
