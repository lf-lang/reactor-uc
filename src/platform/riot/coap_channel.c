#include "reactor-uc/platform/riot/coap_channel.h"
#include "reactor-uc/logging.h"

static ssize_t _connect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  (void)ctx;

  gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  if (self.connected_to_client) {
    // This server is already connected to a client
    return false;
  }

  // Set as connected
  self.connected_to_client = true;

  // Connect successful
  return true;
}

static ssize_t _disconnect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  (void)ctx;

  gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  if (!self.connected_to_client) {
    // This server is not connected to any client
    return false;
  }

  // Set as disconnect
  self.connected_to_client = false;

  // Disconnect successful
  return true;
}

static ssize_t _send_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  (void)ctx;

  gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  // TODO

  return true;
}

static const coap_resource_t _resources[] = {
    {"/connect", COAP_POST, _connect_handler, NULL},
    {"/disconnect", COAP_POST, _disconnect_handler, NULL},
    {"/send", COAP_POST, _send_handler, NULL},
};

/* Adds link format params to resource list */
static ssize_t _encode_link(const coap_resource_t *resource, char *buf, size_t maxlen,
                            coap_link_encoder_ctx_t *context) {
  ssize_t res = gcoap_encode_link(resource, buf, maxlen, context);
  if (res > 0) {
    if (_link_params[context->link_pos] && (strlen(_link_params[context->link_pos]) < (maxlen - res))) {
      if (buf) {
        memcpy(buf + res, _link_params[context->link_pos], strlen(_link_params[context->link_pos]));
      }
      return res + strlen(_link_params[context->link_pos]);
    }
  }

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

  // TODO
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

  // TODO
}

static void CoapChannel_free(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Free");
  CoapChannel *self = (CoapChannel *)untyped_self;

  // TODO
}

void CoapChannel_ctor(CoapChannel *self, const char *local_host, unsigned short local_port, const char *remote_host,
                      unsigned short remote_port) {
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
