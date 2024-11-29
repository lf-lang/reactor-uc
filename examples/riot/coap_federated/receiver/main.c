#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"

#define REMOTE_ADDRESS "fe80::4c48:d8ff:fece:9a93"
#define REMOTE_PROTOCOL_FAMILY AF_INET6

typedef struct {
  int size;
  char msg[512];
} lf_msg_t;

lf_ret_t deserialize_msg_t(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  (void)msg_size;

  lf_msg_t *msg = user_struct;
  memcpy(&msg->size, msg_buf, sizeof(msg->size));
  memcpy(msg->msg, msg_buf + sizeof(msg->size), msg->size);

  return LF_OK;
}

DEFINE_REACTION_STRUCT(Receiver, r, 0);
DEFINE_REACTION_CTOR(Receiver, r, 0);
DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, lf_msg_t, 0);
DEFINE_INPUT_CTOR(Receiver, in, 1, 0, lf_msg_t, 0);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Receiver, r);
  PORT_INSTANCE(Receiver, in, 1);
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

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Receiver);
  INITIALIZE_REACTION(Receiver, r);
  INITIALIZE_INPUT(Receiver, in, 1, in_external);

  // Register reaction as an effect of in
  PORT_REGISTER_EFFECT(self->in, self->r, 1);
}

DEFINE_FEDERATED_INPUT_CONNECTION(Receiver, in, lf_msg_t, 5, MSEC(100), false);

typedef struct {
  FederatedConnectionBundle super;
  CoapUdpIpChannel channel;
  FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(1, 0)
} FEDERATED_CONNECTION_BUNDLE_NAME(Receiver, Sender);

FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  CoapUdpIpChannel_ctor(&self->channel, parent->env, REMOTE_ADDRESS, REMOTE_PROTOCOL_FAMILY);
  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}

typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  FEDERATE_BOOKKEEPING_INSTANCES(1);
  CHILD_INPUT_SOURCES(receiver, in, 1, 1, 0);
} MainRecv;

REACTOR_CTOR_SIGNATURE(MainRecv) {
  REACTOR_CTOR(MainRecv);
  FEDERATE_CTOR_PREAMBLE();
  DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, _receiver_in_args[i]);
  INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
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
