#include "reactor-uc/logging.h"
#include "reactor-uc/platform/posix/tcp_ip_channel.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/serialization.h"
#include <zephyr/net/net_ip.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "../../common/receiver.h"

#define PORT_NUM 8901

DEFINE_FEDERATED_INPUT_CONNECTION(ConnRecv, 1, msg_t, 5, 0, false)

typedef struct {
  FederatedConnectionBundle super;
  TcpIpChannel chan;
  ConnRecv conn;
  FederatedInputConnection *inputs[1];
  deserialize_hook deserialize_hooks[1];
} RecvSenderBundle;

void RecvSenderBundle_ctor(RecvSenderBundle *self, Reactor *parent) {
  ConnRecv_ctor(&self->conn, parent);
  TcpIpChannel_ctor(&self->chan, "192.168.1.100", PORT_NUM, AF_INET, false);
  self->inputs[0] = &self->conn.super;
  self->deserialize_hooks[0] = deserialize_payload_default;

  FederatedConnectionBundle_ctor(&self->super, parent, &self->chan.super, (FederatedInputConnection **)&self->inputs,
                                 self->deserialize_hooks, 1, NULL, NULL, 0);
}

typedef struct {
  Reactor super;
  Receiver receiver;
  RecvSenderBundle bundle;
  FederatedConnectionBundle *_bundles[1];
  Reactor *_children[1];
} MainRecv;

void MainRecv_ctor(MainRecv *self, Environment *env) {
  self->_children[0] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  RecvSenderBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn, self->receiver.in);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);

  self->_bundles[0] = &self->bundle.super;
}

ENTRY_POINT_FEDERATED(MainRecv, FOREVER, true, true, 1, false)

int main() {
  setup_led();
  lf_start();
}