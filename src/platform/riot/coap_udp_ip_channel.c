#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/serialization.h"

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

static void _update_state(CoapUdpIpChannel *self, NetworkChannelState state) {
  // Update the state of the channel itself
  self->state = state;

  // Inform FederatedConnectionBundle about the new state of the channel
  self->federated_connection->network_channel_state_changed(self->federated_connection);
}

static CoapUdpIpChannel *_get_coap_channel_by_remote(const sock_udp_ep_t *remote) {
  CoapUdpIpChannel *channel;
  for (size_t i = 0; i < _env->net_bundles_size; i++) {
    if (_env->net_bundles[i]->net_channel->type == NETWORK_CHANNEL_TYPE_COAP_UDP_IP) {
      channel = (CoapUdpIpChannel *)_env->net_bundles[i]->net_channel;

      if (remote->family == AF_INET6) {
        if (ipv6_addr_equal((ipv6_addr_t *)&channel->remote.addr.ipv6, (ipv6_addr_t *)&remote->addr.ipv6)) {
          return channel;
        }
      } else if (remote->family == AF_INET) {
        if (ipv4_addr_equal((ipv4_addr_t *)&channel->remote.addr.ipv4, (ipv4_addr_t *)&remote->addr.ipv4)) {
          return channel;
        }
      }
    }
  }

  return NULL;
}

static bool _send_coap_message(sock_udp_ep_t *remote, char *path, gcoap_resp_handler_t resp_handler) {
  coap_pkt_t pdu;
  uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];

  // TODO make COAP_TYPE_NON => COAP_TYPE_CON
  size_t len = gcoap_request(&pdu, buf, CONFIG_GCOAP_PDU_BUF_SIZE, COAP_TYPE_NON, path);

  ssize_t bytes_sent = gcoap_req_send(buf, len, remote, NULL, resp_handler, NULL, GCOAP_SOCKET_TYPE_UDP);
  LF_DEBUG(NET, "CoapUdpIpChannel: Sending %d bytes", bytes_sent);
  if (bytes_sent > 0) {
    LF_DEBUG(NET, "CoapUdpIpChannel: Successfully sent");
    return true;
  }

  return false;
}

static bool _send_coap_message_with_payload(CoapUdpIpChannel *self, sock_udp_ep_t *remote, char *path,
                                            gcoap_resp_handler_t resp_handler, const FederateMessage *message) {
  coap_pkt_t pdu;
  // TODO make COAP_TYPE_NON => COAP_TYPE_CON
  unsigned msg_type = COAP_TYPE_NON;

  gcoap_req_init(&pdu, &self->write_buffer[0], CONFIG_GCOAP_PDU_BUF_SIZE, msg_type, path);
  coap_hdr_set_type(pdu.hdr, msg_type);

  coap_opt_add_format(&pdu, COAP_FORMAT_TEXT);
  ssize_t len = coap_opt_finish(&pdu, COAP_OPT_FINISH_PAYLOAD);

  // Serialize message into CoAP payload buffer
  int payload_len = serialize_to_protobuf(message, pdu.payload, pdu.payload_len);

  if (payload_len < 0) {
    LF_ERR(NET, "CoapUdpIpChannel: Could not encode protobuf");
    return LF_ERR;
  }

  if (pdu.payload_len < payload_len) {
    LF_ERR(NET, "CoapUdpIpChannel: Send CoAP message: msg buffer too small (%d < %d)", pdu.payload_len, payload_len);
    return false;
  }

  // Update CoAP packet length based on serialized payload length
  len = len + payload_len;

  ssize_t bytes_sent = gcoap_req_send(self->write_buffer, len, remote, NULL, resp_handler, NULL, GCOAP_SOCKET_TYPE_UDP);
  LF_DEBUG(NET, "CoapUdpIpChannel: Sending %d bytes", bytes_sent);
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
    _update_state(self, NETWORK_CHANNEL_STATE_CLOSED);
  }

  // Success => return 204 (no content)
  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static ssize_t _server_message_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len, coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Server message handler");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapUdpIpChannel: Server message handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Deserialize received message
  deserialize_from_protobuf(&self->output, pdu->payload, pdu->payload_len);
  LF_DEBUG(NET, "CoapUdpIpChannel: Server message handler: Server received message: %s",
           self->output.message.tagged_message.payload.bytes);

  // Call registered receive callback to inform runtime about the new message
  self->receive_callback(self->federated_connection, &self->output);

  // Respond to the other federate
  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static const coap_resource_t _resources[] = {
    {"/connect", COAP_GET, _server_connect_handler, NULL},
    {"/disconnect", COAP_GET, _server_disconnect_handler, NULL},
    {"/message", COAP_GET, _server_message_handler, NULL}, // TODO: Reevaluate GET / POST ETC.
};

static gcoap_listener_t _listener = {&_resources[0], ARRAY_SIZE(_resources), GCOAP_SOCKET_TYPE_UDP, NULL, NULL, NULL};

static lf_ret_t CoapUdpIpChannel_open_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Open connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  /* Server */
  // Do nothing

  /* Client */
  // Do nothing

  _update_state(self, NETWORK_CHANNEL_STATE_OPEN);

  return LF_OK;
}

static void _client_try_connect_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                         const sock_udp_ep_t *remote) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Client try connect callback");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(remote);

  // Failure
  if (memo->state == GCOAP_MEMO_TIMEOUT || coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
    _update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
    return;
  }

  // Success
  _update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
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
    if (!_send_coap_message(&self->remote, "/connect", _client_try_connect_callback)) {
      LF_ERR(NET, "CoapUdpIpChannel: try_connect: Failed to send CoAP message");
      return LF_ERR;
    }
    return LF_IN_PROGRESS;

  case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS:
    return LF_IN_PROGRESS;

  case NETWORK_CHANNEL_STATE_CONNECTION_FAILED:
  case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
    _update_state(self, NETWORK_CHANNEL_STATE_OPEN);
    return LF_TRY_AGAIN;

  case NETWORK_CHANNEL_STATE_UNINITIALIZED:
  case NETWORK_CHANNEL_STATE_CLOSED:
    return LF_ERR;
  }

  return LF_ERR;
}

static lf_ret_t CoapUdpIpChannel_try_reconnect(NetworkChannel *untyped_self) {
  return CoapUdpIpChannel_try_connect(untyped_self);
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
  _update_state(self, NETWORK_CHANNEL_STATE_CLOSED);

  // Inform the other federate that the channel is closed
  _send_coap_message(&self->remote, "/disconnect", _client_close_connection_callback);
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

  // Send message
  if (_send_coap_message_with_payload(self, &self->remote, "/message", _client_send_blocking_callback, message)) {
    return LF_OK;
  }

  return LF_ERR;
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
  (void)self;

  // Do nothing
}

static NetworkChannelState CoapUdpIpChannel_get_connection_state(NetworkChannel *untyped_self) {
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;
  return self->state;
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
  self->super.expected_try_connect_duration = 0;
  self->super.type = NETWORK_CHANNEL_TYPE_COAP_UDP_IP;
  self->super.get_connection_state = CoapUdpIpChannel_get_connection_state;
  self->super.open_connection = CoapUdpIpChannel_open_connection;
  self->super.try_connect = CoapUdpIpChannel_try_connect;
  self->super.try_reconnect = CoapUdpIpChannel_try_reconnect;
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
