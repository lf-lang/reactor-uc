#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"

#define REMOTE_HOST "[::1]"

typedef struct {
  int size;
  char msg[512];
} lf_msg_t;

lf_ret_t deserialize_msg_t(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  lf_msg_t *msg = user_struct;
  memcpy(&msg->size, msg_buf, sizeof(msg->size));
  memcpy(msg->msg, msg_buf + sizeof(msg->size), msg->size);

  return LF_OK;
}

DEFINE_REACTION_STRUCT(Receiver, r, 0);
DEFINE_REACTION_CTOR(Receiver, r, 0);
DEFINE_INPUT_STRUCT(Receiver, in, 1, lf_msg_t, 0);
DEFINE_INPUT_CTOR(Receiver, in, 1, lf_msg_t, 0);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Receiver, r);
  PORT_INSTANCE(Receiver, in);
  int cnt;
  REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
} Receiver;

DEFINE_REACTION_BODY(Receiver, r) {
  SCOPE_SELF(Receiver);
  SCOPE_ENV();
  SCOPE_PORT(Receiver, in);
  printf("Input triggered @ %" PRId64 " with %s size %d\n", env->get_elapsed_logical_time(env), in->value.msg,
         in->value.size);
}

REACTOR_CTOR_SIGNATURE(Receiver) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Receiver);
  INITIALIZE_REACTION(Receiver, r);
  INITIALIZE_INPUT(Receiver, in);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(in, r);
}

DEFINE_FEDERATED_INPUT_CONNECTION(Receiver, in, lf_msg_t, 5, MSEC(100), false);

typedef struct {
  FederatedConnectionBundle super;
  CoapUdpIpChannel channel;
  FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(1, 0)
} FEDERATED_CONNECTION_BUNDLE_NAME(Receiver, Sender);

// TODO This macro does not work because I need env here.
// FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
//   FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
//   CoapUdpIpChannel_ctor(&self->channel, env, REMOTE_HOST);
//   FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
//   INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
// }
void Receiver_Sender_Bundle_ctor(Receiver_Sender_Bundle *self, Environment *env, Reactor *parent) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  CoapUdpIpChannel_ctor(&self->channel, env, REMOTE_HOST);
  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}

// Reactor main
typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Receiver, receiver);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  FEDERATE_BOOKKEEPING_INSTANCES(0, 0, 1, 1);
} MainRecv;

REACTOR_CTOR_SIGNATURE(MainRecv) {
  FEDERATE_CTOR_PREAMBLE();
  REACTOR_CTOR(MainRecv);
  INITIALIZE_CHILD_REACTOR(Receiver, receiver);
  // TODO this function has more arguments now because I need env
  // INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  Receiver_Sender_Bundle_ctor(&self->Receiver_Sender_bundle, env, &self->super);
  self->_bundles[_bundle_idx++] = &self->Receiver_Sender_bundle.super;

  BUNDLE_REGISTER_DOWNSTREAM(Receiver, Sender, receiver, in);
}

ENTRY_POINT_FEDERATED(MainRecv, SEC(1), true, true, 1, false)

void print_ip_addresses(void) {
  gnrc_netif_t *netif = gnrc_netif_iter(NULL);
  char addr_str[IPV6_ADDR_MAX_STR_LEN];

  while (netif) {
    size_t max_addr_count = 4;
    ipv6_addr_t addrs[max_addr_count];
    gnrc_netif_ipv6_addrs_get(netif, addrs, max_addr_count * sizeof(ipv6_addr_t));

    for (size_t i = 0; i < 2; i++) {
      if (ipv6_addr_to_str(addr_str, &addrs[i], sizeof(addr_str))) {
        LF_INFO(NET, "IPv6 address: %s", addr_str);
      }
    }

    netif = gnrc_netif_iter(netif);
  }
}

int main() {
  print_ip_addresses();
  lf_start();
  return 0;
}
