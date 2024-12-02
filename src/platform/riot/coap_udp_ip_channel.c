#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/serialization.h"

#include "net/gcoap.h"
#include "net/sock/util.h"
#include <arpa/inet.h>

static bool _is_globals_initialized = false;
static Environment *_env;

// Forward declarations
static lf_ret_t _client_send_connect_message(CoapUdpIpChannel *self);

static void _update_state(CoapUdpIpChannel *self, NetworkChannelState new_state) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Update State: %d => %d\n", self->state, new_state);

  // Update the state of the channel itself
  mutex_lock(&self->state_mutex);
  self->state = new_state;
  mutex_unlock(&self->state_mutex);

  // Inform runtime about new state
  _env->platform->new_async_event(_env->platform);
}

static void _update_state_if_not(CoapUdpIpChannel *self, NetworkChannelState new_state, NetworkChannelState if_not) {
  // Update the state of the channel itself
  mutex_lock(&self->state_mutex);
  if (self->state != if_not) {
    LF_DEBUG(NET, "CoapUdpIpChannel: Update State: %d => %d\n", self->state, new_state);
    self->state = new_state;
  }
  mutex_unlock(&self->state_mutex);

  // Inform runtime about new state
  _env->platform->new_async_event(_env->platform);
}

static NetworkChannelState _get_state(CoapUdpIpChannel *self) {
  NetworkChannelState state;

  mutex_lock(&self->state_mutex);
  state = self->state;
  mutex_unlock(&self->state_mutex);

  return state;
}

static NetworkChannelState _get_state(CoapUdpIpChannel *self) {
  NetworkChannelState state;

  mutex_lock(&self->state_mutex);
  state = self->state;
  mutex_unlock(&self->state_mutex);

  return state;
}

static CoapUdpIpChannel *_CoapUdpIpChannel_get_coap_channel_by_remote(const sock_udp_ep_t *remote) {
  CoapUdpIpChannel *channel;
  for (size_t i = 0; i < _env->net_bundles_size; i++) {
    if (_env->net_bundles[i]->net_channel->type == NETWORK_CHANNEL_TYPE_COAP_UDP_IP) {
      channel = (CoapUdpIpChannel *)_env->net_bundles[i]->net_channel;

      if (sock_udp_ep_equal(&channel->remote, remote)) {
        return channel;
      }
    }
  }

  LF_ERR(NET, "CoapUdpIpChannel: Channel not found by socket");

  return NULL;
}

static bool _CoapUdpIpChannel_send_coap_message(sock_udp_ep_t *remote, char *path, gcoap_resp_handler_t resp_handler) {
  coap_pkt_t pdu;
  uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];

  size_t len = gcoap_request(&pdu, buf, CONFIG_GCOAP_PDU_BUF_SIZE, COAP_POST, path);
  coap_hdr_set_type(pdu.hdr, COAP_TYPE_CON);

  ssize_t bytes_sent = gcoap_req_send(buf, len, remote, NULL, resp_handler, NULL, GCOAP_SOCKET_TYPE_UDP);
  LF_DEBUG(NET, "CoapUdpIpChannel: Sending %d bytes", bytes_sent);
  if (bytes_sent > 0) {
    LF_DEBUG(NET, "CoapUdpIpChannel: Message sent");
    return true;
  }

  return false;
}

static bool _CoapUdpIpChannel_send_coap_message_with_payload(CoapUdpIpChannel *self, sock_udp_ep_t *remote, char *path,
                                                             gcoap_resp_handler_t resp_handler,
                                                             const FederateMessage *message) {
  coap_pkt_t pdu;

  gcoap_req_init(&pdu, &self->write_buffer[0], CONFIG_GCOAP_PDU_BUF_SIZE, COAP_POST, path);
  coap_hdr_set_type(pdu.hdr, COAP_TYPE_CON);

  coap_opt_add_format(&pdu, COAP_FORMAT_TEXT);
  ssize_t len = coap_opt_finish(&pdu, COAP_OPT_FINISH_PAYLOAD);

  // Serialize message into CoAP payload buffer
  int payload_len = serialize_to_protobuf(message, pdu.payload, pdu.payload_len);

  if (payload_len < 0) {
    LF_ERR(NET, "CoapUdpIpChannel: Could not encode protobuf");
    return false;
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
    LF_DEBUG(NET, "CoapUdpIpChannel: Message sent");
    return true;
  }

  return false;
}

static ssize_t _CoapUdpIpChannel_server_connect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                                                        coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Server connect handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapUdpIpChannel: Server connect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Error => return 503 (service unavailable)
  if (_get_state(self) == NETWORK_CHANNEL_STATE_CLOSED) {
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

static ssize_t _CoapUdpIpChannel_server_disconnect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                                                           coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Server disconnect handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    LF_ERR(NET, "CoapUdpIpChannel: Server disconnect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Update state because it does not make sense to send data to a closed connection.
  if (_get_state(self) != NETWORK_CHANNEL_STATE_UNINITIALIZED && _get_state(self) != NETWORK_CHANNEL_STATE_CLOSED) {
    _update_state(self, NETWORK_CHANNEL_STATE_CLOSED);
  }

  // Success => return 204 (no content)
  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static ssize_t _CoapUdpIpChannel_server_message_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                                                        coap_request_ctx_t *ctx) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Server message handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(ctx->remote);

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
    {"/connect", COAP_POST, _CoapUdpIpChannel_server_connect_handler, NULL},
    {"/disconnect", COAP_POST, _CoapUdpIpChannel_server_disconnect_handler, NULL},
    {"/message", COAP_POST, _CoapUdpIpChannel_server_message_handler, NULL},
};

static gcoap_listener_t _listener = {&_resources[0], ARRAY_SIZE(_resources), GCOAP_SOCKET_TYPE_UDP, NULL, NULL, NULL};

static void _client_open_connection_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                             const sock_udp_ep_t *remote) {
  (void)remote; // This pointer is useless and always NULL
  LF_DEBUG(NET, "CoapUdpIpChannel: Client open connection callback");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(&memo->remote_ep);
  if (memo->state == GCOAP_MEMO_TIMEOUT) {
    // Failure
    LF_ERR(NET, "CoapUdpIpChannel: TIMEOUT => Try to connect again");
    _client_send_connect_message(self); // Try to connect again
  } else if (coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
    // Failure
    LF_ERR(NET, "CoapUdpIpChannel: CONNECTION REJECTED => Try to connect again");
    _client_send_connect_message(self); // Try to connect again
  } else {
    // Success
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
  }
}

static lf_ret_t _client_send_connect_message(CoapUdpIpChannel *self) {
  if (!_send_coap_message(&self->remote, "/connect", _client_open_connection_callback)) {
    _update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
    LF_ERR(NET, "CoapUdpIpChannel: Open connection: Failed to send CoAP message");
    return LF_ERR;
  } else {
    _update_state_if_not(self, NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS, NETWORK_CHANNEL_STATE_CONNECTED);
  }

  return LF_OK;
}

static lf_ret_t CoapUdpIpChannel_open_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Open connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  /* Server */
  // Do nothing. Every CoAP client is also a server in our case.
  // If the client successfully connected to the server of the other federate,
  // then we consider the connection as established
  // The other federate will also connect to our server and after that consider
  // the connection to us as established.

  /* Client */
  return _client_send_connect_message(self);
}

static void _client_close_connection_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                              const sock_udp_ep_t *remote) {
  (void)remote; // This pointer is useless and always NULL
  LF_DEBUG(NET, "CoapUdpIpChannel: Client close connection callback");
  (void)memo;
  (void)pdu;

  // Do nothing
}

static void CoapUdpIpChannel_close_connection(NetworkChannel *untyped_self) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Close connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Immediately close the channel
  _update_state(self, NETWORK_CHANNEL_STATE_CLOSED);

  // Inform the other federate that the channel is closed
  _CoapUdpIpChannel_send_coap_message(&self->remote, "/disconnect", _CoapUdpIpChannel_client_close_connection_callback);
}

static void _client_send_blocking_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                           const sock_udp_ep_t *remote) {
  (void)remote; // This pointer is useless and always NULL
  LF_DEBUG(NET, "CoapUdpIpChannel: Client send blocking callback");
  CoapUdpIpChannel *self = _get_coap_channel_by_remote(&memo->remote_ep);

  if (memo->state == GCOAP_MEMO_TIMEOUT) {
    // Failure
    LF_ERR(NET, "CoapUdpIpChannel: TIMEOUT");
    _update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
  } else if (coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
    // Failure
    LF_ERR(NET, "CoapUdpIpChannel: CONNECTION REJECTED");
    _update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
  }

  self->send_ack_received = true;
}

static lf_ret_t CoapUdpIpChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  LF_DEBUG(NET, "CoapUdpIpChannel: Send blocking");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Send message
  self->send_ack_received = false;
  if (_send_coap_message_with_payload(self, &self->remote, "/message", _client_send_blocking_callback, message)) {
    // Wait until the response handler confirms the ack or times out
    while (!self->send_ack_received) {
      thread_yield_higher();
    }

    if (_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED) {
      return LF_OK;
    } else {
      // Try to connect again
      _client_send_connect_message(self);
    }
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
  return _get_state(self);
}

void CoapUdpIpChannel_ctor(CoapUdpIpChannel *self, Environment *env, const char *remote_address,
                           int remote_protocol_family) {
  assert(self != NULL);
  assert(env != NULL);
  assert(remote_address != NULL);

  // Initialize global coap server if not already done
  if (!_is_globals_initialized) {
    _is_globals_initialized = true;

    // Set environment
    _env = env;

    // Initialize coap server
    gcoap_register_listener(&_listener);
  }

  // Super fields
  self->super.expected_try_connect_duration = MSEC(1000); // TODO: Make this configurable
  self->super.type = NETWORK_CHANNEL_TYPE_COAP_UDP_IP;
  self->super.get_connection_state = CoapUdpIpChannel_get_connection_state;
  self->super.open_connection = CoapUdpIpChannel_open_connection;
  self->super.close_connection = CoapUdpIpChannel_close_connection;
  self->super.send_blocking = CoapUdpIpChannel_send_blocking;
  self->super.register_receive_callback = CoapUdpIpChannel_register_receive_callback;
  self->super.free = CoapUdpIpChannel_free;

  // Concrete fields
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
  self->state_mutex = (mutex_t)MUTEX_INIT;

  // Convert host to udp socket
  if (inet_pton(remote_protocol_family, remote_address, self->remote.addr.ipv6) == 1) {
    self->remote.family = remote_protocol_family;
    self->remote.port = CONFIG_GCOAP_PORT;
  } else {
    LF_ERR(NET, "CoapUdpIpChannel: Error parsing IP-Address");
  }
}
