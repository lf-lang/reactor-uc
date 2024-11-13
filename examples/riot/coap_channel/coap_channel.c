#include "coap_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net/gcoap.h"
#include "net/ipv6/addr.h"
#include "net/ipv4/addr.h"
#include "net/sock/util.h"
#include "od.h"
#include "uri_parser.h"

static bool _is_coap_initialized = false;
static Environment *_env;

static CoapChannel *_get_coap_channel_by_remote(sock_udp_ep_t *remote) {
  CoapChannel *channel;
  for (size_t i = 0; i < _env->net_bundles_size; i++) {
    channel = (CoapChannel *)_env->net_bundles[i]->net_channel;

    if (remote->family == AF_INET6) {
      if (ipv6_addr_equal(&channel->remote.addr.ipv6, &remote->addr.ipv6)) {
        return channel;
      }
    } else if (remote->family == AF_INET) {
      if (ipv4_addr_equal(&channel->remote.addr.ipv4, &remote->addr.ipv4)) {
        return channel;
      }
    }
  }

  return NULL;
}

// static void _resp_handler(const gcoap_request_memo_t *memo, coap_pkt_t *pdu, const sock_udp_ep_t *remote) {
//   (void)remote; /* not interested in the source currently */

//   if (memo->state == GCOAP_MEMO_TIMEOUT) {
//     LF_DEBUG(NET, "gcoap: timeout for msg ID %02u\n", coap_get_id(pdu));
//     return;
//   }

//   char *class_str = (coap_get_code_class(pdu) == COAP_CLASS_SUCCESS) ? "Success" : "Error";
//   LF_DEBUG(NET, "gcoap: response %s, code %1u.%02u", class_str, coap_get_code_class(pdu), coap_get_code_detail(pdu));
//   if (pdu->payload_len) {
//     unsigned content_type = coap_get_content_type(pdu);
//     if (content_type == COAP_FORMAT_TEXT || content_type == COAP_FORMAT_LINK ||
//         coap_get_code_class(pdu) == COAP_CLASS_CLIENT_FAILURE ||
//         coap_get_code_class(pdu) == COAP_CLASS_SERVER_FAILURE) {
//       /* Expecting diagnostic payload in failure cases */
//       LF_DEBUG(NET, ", %u bytes\n%.*s\n", pdu->payload_len, pdu->payload_len, (char *)pdu->payload);
//     } else {
//       LF_DEBUG(NET, ", %u bytes\n", pdu->payload_len);
//       od_hex_dump(pdu->payload, pdu->payload_len, OD_WIDTH_DEFAULT);
//     }
//   } else {
//     LF_DEBUG(NET, ", empty payload\n");
//   }
// }

static bool _send_coap_message(sock_udp_ep_t *remote, char *path, gcoap_resp_handler_t resp_handler) {
  coap_pkt_t pdu;
  uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];
  unsigned msg_type = COAP_TYPE_NON;

  gcoap_req_init(&pdu, &buf[0], CONFIG_GCOAP_PDU_BUF_SIZE, msg_type, path);
  coap_hdr_set_type(pdu.hdr, msg_type);
  ssize_t len = coap_opt_finish(&pdu, COAP_OPT_FINISH_NONE);
  ssize_t bytes_sent = gcoap_req_send(buf, len, remote, NULL, resp_handler, NULL, GCOAP_SOCKET_TYPE_UDP);
  LF_DEBUG(NET, "CoapChannel: %d", bytes_sent);
  if (bytes_sent > 0) {
    LF_DEBUG(NET, "CoapChannel: Successfully sent");
    return true;
  }

  return false;
}

static ssize_t _server_connect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapChannel: Server connect handler");
  CoapChannel *self = _get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapChannel: Server connect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Error => return 503 (service unavailable)
  if (self->state == NETWORK_CHANNEL_STATE_CLOSED) {
    LF_ERR(NET, "CoapChannel: Server connect handler: Channel is closed");
    return gcoap_response(pdu, buf, len, COAP_CODE_SERVICE_UNAVAILABLE);
  }

  // Do not update the state here.
  // The connected state is only determined by if the client succeeds
  // to connect to the server of the other federate.
  // This way it is guaranteed that the servers and clients on both federates
  // work correctly (at least for connecting).

  // Success => return 204 (no content)
  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static ssize_t _server_disconnect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapChannel: Server disconnect handler");
  CoapChannel *self = _get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapChannel: Server disconnect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Update state because it does not make sense to send data to a closed connection.
  // Only set state to disconnected if not already in more restrictive state
  if (self->state != NETWORK_CHANNEL_STATE_UNINITIALIZED && self->state != NETWORK_CHANNEL_STATE_CLOSED) {
    self->state = NETWORK_CHANNEL_STATE_DISCONNECTED;
  }

  // Success => return 204 (no content)
  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static ssize_t _server_message_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapChannel: Server message handler");
  CoapChannel *self = _get_coap_channel_by_remote(ctx->remote);

  gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  // TODO

  // gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
  // coap_opt_add_format(pdu, COAP_FORMAT_TEXT);
  // size_t resp_len = coap_opt_finish(pdu, COAP_OPT_FINISH_PAYLOAD);

  // /* write the RIOT board name in the response buffer */
  // if (pdu->payload_len >= strlen(RIOT_BOARD)) {
  //   memcpy(pdu->payload, RIOT_BOARD, strlen(RIOT_BOARD));
  //   return resp_len + strlen(RIOT_BOARD);
  // } else {
  //   puts("gcoap_cli: msg buffer too small");
  //   return gcoap_response(pdu, buf, len, COAP_CODE_INTERNAL_SERVER_ERROR);
  // }

  return true;
}

static const coap_resource_t _resources[] = {
    {"/connect", COAP_GET, _server_connect_handler, NULL},
    {"/disconnect", COAP_GET, _server_disconnect_handler, NULL},
    {"/message", COAP_GET, _server_message_handler, NULL},
};

static gcoap_listener_t _listener = {&_resources[0], ARRAY_SIZE(_resources), GCOAP_SOCKET_TYPE_UDP, NULL, NULL, NULL};

static lf_ret_t CoapChannel_open_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Open connection");
  CoapChannel *self = (CoapChannel *)untyped_self;

  /* Server */
  // Do nothing

  /* Client */
  // Do nothing

  self->state = NETWORK_CHANNEL_STATE_OPEN;
}

static void _client_try_connect_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                         const sock_udp_ep_t *remote) {
  LF_DEBUG(NET, "CoapChannel: Client try connect callback");
  CoapChannel *self = _get_coap_channel_by_remote(remote);

  // Failure
  if (memo->state == GCOAP_MEMO_TIMEOUT || coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
    self->state = NETWORK_CHANNEL_STATE_DISCONNECTED;
    return;
  }

  // Success
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t CoapChannel_try_connect(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Try connect");
  CoapChannel *self = (CoapChannel *)untyped_self;

  /* Server */
  // Do nothing. Every CoAP client is also a server in our case.
  // If the client successfully connected to the server of the other federate,
  // then we consider the connection as established
  // The other federate will also connect to our server and after that consider
  // the connection to us as established.

  /* Client */
  switch (self->state) {
  case NETWORK_CHANNEL_STATE_CONNECTED:
    return LF_OK;

  case NETWORK_CHANNEL_STATE_OPEN:
    if (!_send_coap_message(&self->remote, "/connect", _client_try_connect_callback)) {
      LF_ERR(NET, "CoapChannel: try_connect: Failed to send CoAP message");
      return LF_ERR;
    }
    return LF_IN_PROGRESS;

  case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS:
    return LF_IN_PROGRESS;

  case NETWORK_CHANNEL_STATE_DISCONNECTED:
  case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
    self->state = NETWORK_CHANNEL_STATE_OPEN;
    return LF_TRY_AGAIN;

  case NETWORK_CHANNEL_STATE_UNINITIALIZED:
  case NETWORK_CHANNEL_STATE_CLOSED:
    return LF_ERR;
  }
}

static void _client_close_connection_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                              const sock_udp_ep_t *remote) {
  LF_DEBUG(NET, "CoapChannel: Client close connection callback");
  (void)memo;
  (void)pdu;
  (void)remote;

  // Do nothing
}

static void CoapChannel_close_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapChannel: Close connection");
  CoapChannel *self = (CoapChannel *)untyped_self;

  // Immediately close the channel
  self->state = NETWORK_CHANNEL_STATE_CLOSED;

  // Inform the other federate that the channel is closed
  _send_coap_message(&self->remote, "/disconnect", _client_close_connection_callback);
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

  // Do nothing
}

void CoapChannel_ctor(CoapChannel *self, Environment *env, const char *remote_host) {
  // Initialize global coap server it not already done
  if (!_is_coap_initialized) {
    _is_coap_initialized = true;

    // Set environment
    _env = env;

    // Initialize coap server
    gcoap_register_listener(&_listener);
  }

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
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;

  // Convert host to udp socket
  sock_udp_name2ep(&self->remote, remote_host);
  if (self->remote.port == 0) {
    self->remote.port = CONFIG_GCOAP_PORT;
  }
}
