#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"

#define REMOTE_ADDRESS "fe80::2882:1bff:fe2d:1362"
#define REMOTE_PROTOCOL_FAMILY AF_INET6

typedef struct {
  int size;
  char msg[512];
} lf_msg_t;

size_t serialize_msg_t(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  (void)user_struct_size;
  const lf_msg_t *msg = user_struct;

  memcpy(msg_buf, &msg->size, sizeof(msg->size));
  memcpy(msg_buf + sizeof(msg->size), msg->msg, msg->size);

  return sizeof(msg->size) + msg->size;
}

DEFINE_TIMER_STRUCT(Sender, t, 1, 0)
DEFINE_TIMER_CTOR(Sender, t, 1, 0)
DEFINE_REACTION_STRUCT(Sender, r, 1)
DEFINE_REACTION_CTOR(Sender, r, 0)
DEFINE_OUTPUT_STRUCT(Sender, out, 1, lf_msg_t)
DEFINE_OUTPUT_CTOR(Sender, out, 1)

typedef struct {
  Reactor super;
  TIMER_INSTANCE(Sender, t);
  REACTION_INSTANCE(Sender, r);
  PORT_INSTANCE(Sender, out, 1);
  REACTOR_BOOKKEEPING_INSTANCES(1, 2, 0);
} Sender;

DEFINE_REACTION_BODY(Sender, r) {
  SCOPE_SELF(Sender);
  SCOPE_ENV();
  SCOPE_PORT(Sender, out);

  printf("Timer triggered @ %" PRId64 "\n", env->get_elapsed_logical_time(env));
  lf_msg_t val;
  strcpy(val.msg, "Hello From Sender");
  val.size = sizeof("Hello From Sender");
  lf_set(out, val);
}

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs *out_external) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Sender);
  INITIALIZE_REACTION(Sender, r);
  INITIALIZE_TIMER(Sender, t, MSEC(0), SEC(1));
  INITIALIZE_OUTPUT(Sender, out, 1, out_external);

  TIMER_REGISTER_EFFECT(self->t, self->r);
  PORT_REGISTER_SOURCE(self->out, self->r, 1);
}

DEFINE_FEDERATED_OUTPUT_CONNECTION(Sender, out, lf_msg_t, 1)

typedef struct {
  FederatedConnectionBundle super;
  CoapUdpIpChannel channel;
  FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0, 1);
} FEDERATED_CONNECTION_BUNDLE_NAME(Sender, Receiver);

FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver) {
  FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  CoapUdpIpChannel_ctor(&self->channel, parent->env, REMOTE_ADDRESS, REMOTE_PROTOCOL_FAMILY);
  FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);
}

// Reactor main
typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Sender, sender, 1);
  FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver);
  FEDERATE_BOOKKEEPING_INSTANCES(1);
  CHILD_OUTPUT_CONNECTIONS(sender, out, 1, 1, 1);
  CHILD_OUTPUT_EFFECTS(sender, out, 1, 1, 0);
  CHILD_OUTPUT_OBSERVERS(sender, out, 1, 1, 0);
} MainSender;

REACTOR_CTOR_SIGNATURE(MainSender) {
  REACTOR_CTOR(MainSender);
  FEDERATE_CTOR_PREAMBLE();
  DEFINE_CHILD_OUTPUT_ARGS(sender, out, 1, 1);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, 1, _sender_out_args[i]);
  INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver);
  BUNDLE_REGISTER_UPSTREAM(Sender, Receiver, sender, out);
}
ENTRY_POINT_FEDERATED(MainSender, SEC(1), true, false, 1, true)

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
