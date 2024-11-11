#include "coap_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net/gcoap.h"
#include "net/sock/util.h"
#include "od.h"
#include "uri_parser.h"

static Environment *env;

static CoapChannel *_get_coap_channel_by_local_port(unsigned short port) {
  CoapChannel *channel;
  for (size_t i = 0; i < env->net_bundles_size; i++) {
    channel = (CoapChannel *)env->net_bundles[i]->net_channel;
    if (channel->local_port == port) {
      return channel;
    }
  }

  return NULL;
}

static int _uristr2remote(const char *uri, sock_udp_ep_t *remote, const char **path, char *buf, size_t buf_len) {
  if (strlen(uri) >= buf_len) {
    LF_ERR(NET, "URI too long");
    return 1;
  }
  uri_parser_result_t urip;
  if (uri_parser_process(&urip, uri, strlen(uri))) {
    LF_ERR(NET, "'%s' is not a valid URI\n", uri);
    return 1;
  }
  memcpy(buf, urip.host, urip.host_len);
  buf[urip.host_len] = '\0';
  if (urip.port_str_len) {
    strcat(buf, ":");
    strncat(buf, urip.port_str, urip.port_str_len);
    buf[urip.host_len + 1 + urip.port_str_len] = '\0';
  }
  if (sock_udp_name2ep(remote, buf) != 0) {
    LF_ERR(NET, "Could not resolve address '%s'\n", buf);
    return -1;
  }
  if (remote->port == 0) {
    remote->port = !strncmp("coaps", urip.scheme, 5) ? CONFIG_GCOAPS_PORT : CONFIG_GCOAP_PORT;
  }
  if (path) {
    *path = urip.path;
  }
  strcpy(buf, uri);
  return 0;
}

static ssize_t _connect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  printf("CONNECT_HANDLER: Local port: %d, Remote port: %d\n", ctx->local->port, ctx->remote->port);
  CoapChannel *self = _get_coap_channel_by_local_port(ctx->local->port);

  gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  if (self->connected_to_client) {
    // This server is already connected to a client
    return false;
  }

  // Set as connected
  self->connected_to_client = true;

  // Connect successful
  return true;
}

static ssize_t _disconnect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  printf("CONNECT_HANDLER: Local port: %d, Remote port: %d\n", ctx->local->port, ctx->remote->port);
  CoapChannel *self = _get_coap_channel_by_local_port(ctx->local->port);

  gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  if (!self->connected_to_client) {
    // This server is not connected to any client
    return false;
  }

  // Set as disconnect
  self->connected_to_client = false;

  // Disconnect successful
  return true;
}

static ssize_t _message_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  printf("CONNECT_HANDLER: Local port: %d, Remote port: %d\n", ctx->local->port, ctx->remote->port);
  CoapChannel *self = _get_coap_channel_by_local_port(ctx->local->port);

  gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  // TODO

  return true;
}

static const coap_resource_t _resources[] = {
    {"/connect", COAP_POST, _connect_handler, NULL},
    {"/disconnect", COAP_POST, _disconnect_handler, NULL},
    {"/message", COAP_POST, _message_handler, NULL},
};

/* Adds link format params to resource list */
static ssize_t _encode_link(const coap_resource_t *resource, char *buf, size_t maxlen,
                            coap_link_encoder_ctx_t *context) {
  ssize_t res = gcoap_encode_link(resource, buf, maxlen, context);

  return res;
}

static gcoap_listener_t _listener = {
    &_resources[0], ARRAY_SIZE(_resources), GCOAP_SOCKET_TYPE_UNDEF, _encode_link, NULL, NULL};

static lf_ret_t CoapChannel_open_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Open connection");
  CoapChannel *self = (CoapChannel *)untyped_self;

  /* Server */
  gcoap_register_listener(&_listener);

  /* Client */
  // Do nothing
}

static lf_ret_t CoapChannel_try_connect(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Try connect");
  CoapChannel *self = (CoapChannel *)untyped_self;

  /* Server */
  // Do nothing

  /* Client */
  coap_pkt_t pdu;
  uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];
  int len = gcoap_request(&pdu, buf, CONFIG_GCOAP_PDU_BUF_SIZE, COAP_METHOD_POST, "/connect");

  // ssize_t bytes_sent = gcoap_req_send(buf, len, &self->remote, NULL, _resp_handler, ctx, tl);

  // if (bytes_sent > 0) {
  // Successfully send
  // }
}

static void CoapChannel_close_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Close connection");
  CoapChannel *self = (CoapChannel *)untyped_self;

  // TODO
}

static lf_ret_t CoapChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  LF_DEBUG(NET, "CoapChannel: Send blocking");
  CoapChannel *self = (CoapChannel *)untyped_self;

  // TODO
}

static void CoapChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                  void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                           const FederateMessage *msg),
                                                  FederatedConnectionBundle *conn) {
  LF_INFO(NET, "CoapChannel: Register receive callback");
  CoapChannel *self = (CoapChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

static void CoapChannel_free(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Free");
  CoapChannel *self = (CoapChannel *)untyped_self;

  // TODO
}

void CoapChannel_ctor(CoapChannel *self, Environment *env, const char *local_host, unsigned short local_port,
                      const char *remote_host, unsigned short remote_port) {
  // Set environment
  env = env;

  // Super fields
  self->super.open_connection = CoapChannel_open_connection;
  self->super.try_connect = CoapChannel_try_connect;
  self->super.close_connection = CoapChannel_close_connection;
  self->super.send_blocking = CoapChannel_send_blocking;
  self->super.register_receive_callback = CoapChannel_register_receive_callback;
  self->super.free = CoapChannel_free;

  // Concrete fields
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->local_host = local_host;
  self->local_port = local_port;
  self->remote_host = remote_host;
  self->remote_port = remote_port;
  self->connected_to_server = false;
  self->connected_to_client = false;
}
