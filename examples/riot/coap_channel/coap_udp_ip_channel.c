#include "coap_udp_ip_channel.h"
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

static CoapUdpIpChannel *_get_coap_channel_by_remote(sock_udp_ep_t *remote) {
  CoapUdpIpChannel *channel;
  for (size_t i = 0; i < _env->net_bundles_size; i++) {
    channel = (CoapUdpIpChannel *)_env->net_bundles[i]->net_channel;

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

static bool _send_coap_message(sock_udp_ep_t *remote, char *path, gcoap_resp_handler_t resp_handler, void *payload,
                               size_t payload_len) {
  coap_pkt_t pdu;
  uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];
  unsigned msg_type = COAP_TYPE_NON; // TODO make CON

  gcoap_req_init(&pdu, &buf[0], CONFIG_GCOAP_PDU_BUF_SIZE, msg_type, path);
  coap_hdr_set_type(pdu.hdr, msg_type);

  ssize_t len;
  if (payload != NULL) {
    coap_opt_add_format(&pdu, COAP_FORMAT_TEXT);
    len = coap_opt_finish(&pdu, COAP_OPT_FINISH_PAYLOAD);

    if (pdu.payload_len < payload_len) {
      LF_ERR(NET, "CoapUdpIpChannel: Send coap message: msg buffer too small (%d < %d)\n", pdu.payload_len,
             payload_len);
      return false;
    }

    memcpy(pdu.payload, payload, payload_len);
    len = len + payload_len;
  } else {
    len = coap_opt_finish(&pdu, COAP_OPT_FINISH_NONE);
  }

  ssize_t bytes_sent = gcoap_req_send(buf, len, remote, NULL, resp_handler, NULL, GCOAP_SOCKET_TYPE_UDP);
  LF_DEBUG(NET, "CoapUdpIpChannel: %d", bytes_sent);
  if (bytes_sent > 0) {
    LF_DEBUG(NET, "CoapUdpIpChannel: Successfully sent");
    return true;
  }

  return false;
}

static ssize_t _server_connect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Server connect handler");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapUdpIpChannel: Server connect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Error => return 503 (service unavailable)
  if (self->state == NETWORK_CHANNEL_STATE_CLOSED) {
    LF_ERR(NET, "CoapUdpIpChannel: Server connect handler: Channel is closed");
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
  LF_DEBUG(NET, "CoapUdpIpChannel: Server disconnect handler");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapUdpIpChannel: Server disconnect handler: Client has unknown IP address");
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
  LF_DEBUG(NET, "CoapUdpIpChannel: Server message handler");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapUdpIpChannel: Server disconnect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // TODO handle message
  printf("Server received message: %s\n", (char *)pdu->payload);

  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static const coap_resource_t _resources[] = {
    {"/connect", COAP_GET, _server_connect_handler, NULL},
    {"/disconnect", COAP_GET, _server_disconnect_handler, NULL},
    {"/message", COAP_GET, _server_message_handler, NULL},
};

static gcoap_listener_t _listener = {&_resources[0], ARRAY_SIZE(_resources), GCOAP_SOCKET_TYPE_UDP, NULL, NULL, NULL};

static lf_ret_t CoapUdpIpChannel_open_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Open connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  /* Server */
  // Do nothing

  /* Client */
  // Do nothing

  self->state = NETWORK_CHANNEL_STATE_OPEN;
}

static void _client_try_connect_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                         const sock_udp_ep_t *remote) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Client try connect callback");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(remote);

  // Failure
  if (memo->state == GCOAP_MEMO_TIMEOUT || coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
    self->state = NETWORK_CHANNEL_STATE_DISCONNECTED;
    return;
  }

  // Success
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t CoapUdpIpChannel_try_connect(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Try connect");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

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
    if (!_send_coap_message(&self->remote, "/connect", _client_try_connect_callback, NULL, 0)) {
      LF_ERR(NET, "CoapUdpIpChannel: try_connect: Failed to send CoAP message");
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
  LF_DEBUG(NET, "CoapUdpIpChannel: Client close connection callback");
  (void)memo;
  (void)pdu;
  (void)remote;

  // Do nothing
}

static void CoapUdpIpChannel_close_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Close connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Immediately close the channel
  self->state = NETWORK_CHANNEL_STATE_CLOSED;

  // Inform the other federate that the channel is closed
  _send_coap_message(&self->remote, "/disconnect", _client_close_connection_callback, NULL, 0);
}

static void _client_send_blocking_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                           const sock_udp_ep_t *remote) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Client send blocking callback");
  (void)memo;
  (void)pdu;
  (void)remote;

  // Do nothing
}

static lf_ret_t CoapUdpIpChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Send blocking");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  char payload[] = "Test message";
  size_t payload_len = sizeof(payload);

  // TODO send real message

  _send_coap_message(&self->remote, "/message", _client_send_blocking_callback, payload, payload_len);
}

static void CoapUdpIpChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                       void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                                const FederateMessage *msg),
                                                       FederatedConnectionBundle *conn) {
  LF_INFO(NET, "CoapUdpIpChannel: Register receive callback");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

static void CoapUdpIpChannel_free(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Free");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Do nothing
}

void CoapUdpIpChannel_ctor(CoapUdpIpChannel *self, Environment *env, const char *remote_host) {
  // Initialize global coap server it not already done
  if (!_is_coap_initialized) {
    _is_coap_initialized = true;

    // Set environment
    _env = env;

    // Initialize coap server
    gcoap_register_listener(&_listener);
  }

  // Super fields
  self->super.open_connection = CoapUdpIpChannel_open_connection;
  self->super.try_connect = CoapUdpIpChannel_try_connect;
  self->super.close_connection = CoapUdpIpChannel_close_connection;
  self->super.send_blocking = CoapUdpIpChannel_send_blocking;
  self->super.register_receive_callback = CoapUdpIpChannel_register_receive_callback;
  self->super.free = CoapUdpIpChannel_free;

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
