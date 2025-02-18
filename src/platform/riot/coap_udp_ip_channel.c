#include "reactor-uc/platform/riot/coap_udp_ip_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/serialization.h"

#include "net/gcoap.h"
#include "net/sock/util.h"
#include "thread.h"
#include <arpa/inet.h>

#define COAP_UDP_IP_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)

char _connection_thread_stack[THREAD_STACKSIZE_MAIN];
int _connection_thread_pid = 0;
static bool _coap_is_globals_initialized = false;

static void _CoapUdpIpChannel_update_state(CoapUdpIpChannel *self, NetworkChannelState new_state) {
  COAP_UDP_IP_CHANNEL_DEBUG("Update state: %s => %s", NetworkChannel_state_to_string(self->state),
                            NetworkChannel_state_to_string(new_state));

  // Store old state
  NetworkChannelState old_state = self->state;

  // Update the state of the channel to its new state
  mutex_lock(&self->state_mutex);
  self->state = new_state;
  mutex_unlock(&self->state_mutex);

  // Inform runtime about new state if it changed from or to NETWORK_CHANNEL_STATE_CONNECTED
  if ((old_state == NETWORK_CHANNEL_STATE_CONNECTED && new_state != NETWORK_CHANNEL_STATE_CONNECTED) ||
      (old_state != NETWORK_CHANNEL_STATE_CONNECTED && new_state == NETWORK_CHANNEL_STATE_CONNECTED)) {
    _lf_environment->platform->new_async_event(_lf_environment->platform);
  }

  // Let connection thread evaluate new state of this channel
  msg_t msg = {
      .type = 0,
      .content.ptr = self,
  };
  msg_try_send(&msg, _connection_thread_pid);
}

static void _CoapUdpIpChannel_update_state_if_not(CoapUdpIpChannel *self, NetworkChannelState new_state,
                                                  NetworkChannelState if_not) {
  // Update the state of the channel itself
  mutex_lock(&self->state_mutex);
  if (self->state != if_not) {
    COAP_UDP_IP_CHANNEL_DEBUG("Update state: %s => %s", NetworkChannel_state_to_string(self->state),
                              NetworkChannel_state_to_string(new_state));
    self->state = new_state;
  }
  mutex_unlock(&self->state_mutex);

  // Inform runtime about new state
  _lf_environment->platform->new_async_event(_lf_environment->platform);
}

static NetworkChannelState _CoapUdpIpChannel_get_state(CoapUdpIpChannel *self) {
  NetworkChannelState state;

  mutex_lock(&self->state_mutex);
  state = self->state;
  mutex_unlock(&self->state_mutex);

  return state;
}

static CoapUdpIpChannel *_CoapUdpIpChannel_get_coap_channel_by_remote(const sock_udp_ep_t *remote) {
  CoapUdpIpChannel *channel;
  for (size_t i = 0; i < _lf_environment->net_bundles_size; i++) {
    if (_lf_environment->net_bundles[i]->encryption_layer->network_channel->type == NETWORK_CHANNEL_TYPE_COAP_UDP_IP) {
      channel = (CoapUdpIpChannel *)_lf_environment->net_bundles[i]->encryption_layer->network_channel;

      if (sock_udp_ep_equal(&channel->remote, remote)) {
        return channel;
      }
    }
  }

  char remote_addr_str[IPV6_ADDR_MAX_STR_LEN];
  sock_udp_ep_fmt(remote, remote_addr_str, NULL);

  COAP_UDP_IP_CHANNEL_ERR("Channel not found by socket (addr=%s)", remote_addr_str);
  return NULL;
}

static bool _CoapUdpIpChannel_send_coap_message(sock_udp_ep_t *remote, char *path, gcoap_resp_handler_t resp_handler) {
  coap_pkt_t pdu;
  uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE];

  size_t len = gcoap_request(&pdu, buf, CONFIG_GCOAP_PDU_BUF_SIZE, COAP_POST, path);
  coap_hdr_set_type(pdu.hdr, COAP_TYPE_CON);

  ssize_t bytes_sent = gcoap_req_send(buf, len, remote, NULL, resp_handler, NULL, GCOAP_SOCKET_TYPE_UDP);
  if (bytes_sent > 0) {
    COAP_UDP_IP_CHANNEL_DEBUG("Sending %d bytes", bytes_sent);
    COAP_UDP_IP_CHANNEL_DEBUG("CoAP Message sent");
    return true;
  }

  COAP_UDP_IP_CHANNEL_ERR("Failed to send CoAP message");
  return false;
}

static bool _CoapUdpIpChannel_send_coap_message_with_payload(CoapUdpIpChannel *self, sock_udp_ep_t *remote, char *path,
                                                             gcoap_resp_handler_t resp_handler, const char *message,
                                                             size_t message_size) {
  coap_pkt_t pdu;

  gcoap_req_init(&pdu, self->write_buffer, CONFIG_GCOAP_PDU_BUF_SIZE, COAP_POST, path);
  coap_hdr_set_type(pdu.hdr, COAP_TYPE_CON);

  coap_opt_add_format(&pdu, COAP_FORMAT_TEXT);
  ssize_t len = coap_opt_finish(&pdu, COAP_OPT_FINISH_PAYLOAD);

  memcpy(message, pdu.payload, message_size);
  // if (pdu.payload_len < message_size) {
  //   COAP_UDP_IP_CHANNEL_ERR("Send CoAP message: msg buffer too small (%d < %d)", pdu.payload_len, message_size);
  //   return false;
  // }

  // Update CoAP packet length based on serialized payload length
  len = len + message_size;

  ssize_t bytes_sent = gcoap_req_send(self->write_buffer, len, remote, NULL, resp_handler, NULL, GCOAP_SOCKET_TYPE_UDP);
  COAP_UDP_IP_CHANNEL_DEBUG("Sending %d bytes", bytes_sent);
  if (bytes_sent > 0) {
    COAP_UDP_IP_CHANNEL_DEBUG("CoAP Message sent");
    return true;
  }

  COAP_UDP_IP_CHANNEL_ERR("Failed to send CoAP message");
  return false;
}

static ssize_t _CoapUdpIpChannel_server_connect_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                                                        coap_request_ctx_t *ctx) {
  COAP_UDP_IP_CHANNEL_DEBUG("Server connect handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server connect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Error => return 503 (service unavailable)
  if (_CoapUdpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CLOSED) {
    COAP_UDP_IP_CHANNEL_ERR("Server connect handler: Channel is closed");
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
  COAP_UDP_IP_CHANNEL_DEBUG("Server disconnect handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server disconnect handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Update state because it does not make sense to send data to a closed connection.
  if (_CoapUdpIpChannel_get_state(self) != NETWORK_CHANNEL_STATE_UNINITIALIZED &&
      _CoapUdpIpChannel_get_state(self) != NETWORK_CHANNEL_STATE_CLOSED) {
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CLOSED);
  }

  // Success => return 204 (no content)
  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static ssize_t _CoapUdpIpChannel_server_message_handler(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                                                        coap_request_ctx_t *ctx) {
  COAP_UDP_IP_CHANNEL_DEBUG("Server message handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(ctx->remote);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server message handler: Client has unknown IP address");
    return gcoap_response(pdu, buf, len, COAP_CODE_UNAUTHORIZED);
  }

  // Deserialize received message
  MessageFraming *message_frame = (MessageFraming *)buf;
  COAP_UDP_IP_CHANNEL_DEBUG("Server message handler: Server received message of size frame size: %i len: %i",
                            message_frame->message_size, len);

  // Call registered receive callback to inform runtime about the new message
  self->receive_callback(self->encryption_layer, (char *)buf, message_frame->message_size);

  // Respond to the other federate
  return gcoap_response(pdu, buf, len, COAP_CODE_204);
}

static const coap_resource_t _resources[] = {
    {"/connect", COAP_POST, _CoapUdpIpChannel_server_connect_handler, NULL},
    {"/disconnect", COAP_POST, _CoapUdpIpChannel_server_disconnect_handler, NULL},
    {"/message", COAP_POST, _CoapUdpIpChannel_server_message_handler, NULL},
};

static gcoap_listener_t _listener = {&_resources[0], ARRAY_SIZE(_resources), GCOAP_SOCKET_TYPE_UDP, NULL, NULL, NULL};

static void _CoapUdpIpChannel_client_open_connection_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                                              const sock_udp_ep_t *remote) {
  (void)remote; // This pointer is useless and always NULL
  COAP_UDP_IP_CHANNEL_DEBUG("Client open connection callback");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(&memo->remote_ep);
  if (memo->state == GCOAP_MEMO_TIMEOUT) {
    // Failure
    COAP_UDP_IP_CHANNEL_ERR("TIMEOUT => Try to connect again");
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
  } else if (coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
    // Failure
    COAP_UDP_IP_CHANNEL_ERR("CONNECTION REJECTED => Try to connect again");
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
  } else {
    // Success
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
  }
}

static lf_ret_t _CoapUdpIpChannel_client_send_connect_message(CoapUdpIpChannel *self) {
  if (!_CoapUdpIpChannel_send_coap_message(&self->remote, "/connect",
                                           _CoapUdpIpChannel_client_open_connection_callback)) {
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
    COAP_UDP_IP_CHANNEL_ERR("Open connection: Failed to send CoAP message");
    return LF_ERR;
  } else {
    _CoapUdpIpChannel_update_state_if_not(self, NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS,
                                          NETWORK_CHANNEL_STATE_CONNECTED);
  }

  return LF_OK;
}

static lf_ret_t CoapUdpIpChannel_open_connection(NetworkChannel *untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Open connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  /* Server */
  // Do nothing. Every CoAP client is also a server in our case.
  // If the client successfully connected to the server of the other federate,
  // then we consider the connection as established
  // The other federate will also connect to our server and after that consider
  // the connection to us as established.

  /* Client */
  _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_OPEN);
  return LF_OK;
}

static void _CoapUdpIpChannel_client_close_connection_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                                               const sock_udp_ep_t *remote) {
  (void)remote; // This pointer is useless and always NULL
  COAP_UDP_IP_CHANNEL_DEBUG("Client close connection callback");
  (void)memo;
  (void)pdu;

  // Do nothing
}

static void CoapUdpIpChannel_close_connection(NetworkChannel *untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Close connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Immediately close the channel
  _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CLOSED);

  // Inform the other federate that the channel is closed
  _CoapUdpIpChannel_send_coap_message(&self->remote, "/disconnect", _CoapUdpIpChannel_client_close_connection_callback);
}

static void _client_send_blocking_callback(const gcoap_request_memo_t *memo, coap_pkt_t *pdu,
                                           const sock_udp_ep_t *remote) {
  (void)remote; // This pointer is useless and always NULL
  COAP_UDP_IP_CHANNEL_DEBUG("Client send blocking callback");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_remote(&memo->remote_ep);

  if (memo->state == GCOAP_MEMO_TIMEOUT) {
    // Failure
    COAP_UDP_IP_CHANNEL_ERR("TIMEOUT");
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
  } else if (coap_get_code_class(pdu) != COAP_CLASS_SUCCESS) {
    // Failure
    COAP_UDP_IP_CHANNEL_ERR("CONNECTION REJECTED");
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
  }

  self->send_ack_received = true;
}

static lf_ret_t CoapUdpIpChannel_send_blocking(NetworkChannel *untyped_self, const char *message, size_t message_size) {
  COAP_UDP_IP_CHANNEL_DEBUG("Send blocking");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Send message
  self->send_ack_received = false;
  if (_CoapUdpIpChannel_send_coap_message_with_payload(self, &self->remote, "/message", _client_send_blocking_callback,
                                                       message, message_size)) {
    // Wait until the response handler confirms the ack or times out
    // TODO: Instead of waiting for THIS message to be acked, we should wait for the previous message to be acked.
    while (!self->send_ack_received) {
      thread_yield_higher();
    }

    if (_CoapUdpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED) {
      return LF_OK;
    }
  }

  return LF_ERR;
}

static void CoapUdpIpChannel_register_receive_callback(NetworkChannel *untyped_self, EncryptionLayer *encryption_layer,
                                                       void (*receive_callback)(EncryptionLayer *encryption_layer,
                                                                                const char *msg, ssize_t msg_size)) {
  COAP_UDP_IP_CHANNEL_INFO("Register receive callback");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->encryption_layer = encryption_layer;
}

static void CoapUdpIpChannel_free(NetworkChannel *untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Free");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;
  (void)self;

  // Do nothing
}

static bool CoapUdpIpChannel_is_connected(NetworkChannel *untyped_self) {
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;
  return _CoapUdpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED;
}

void *_CoapUdpIpChannel_connection_thread(void *arg) {
  COAP_UDP_IP_CHANNEL_DEBUG("Start connection thread");
  (void)arg;
  msg_t m;

  while (true) {
    msg_receive(&m);

    CoapUdpIpChannel *self = m.content.ptr;

    switch (self->state) {
    case NETWORK_CHANNEL_STATE_OPEN: {
      /* try to connect */
      _CoapUdpIpChannel_client_send_connect_message(self);
    } break;

    case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS:
      /* nothing to do */
      break;

    case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
    case NETWORK_CHANNEL_STATE_CONNECTION_FAILED: {
      /* try to reconnect */
      _CoapUdpIpChannel_client_send_connect_message(self);
    } break;

    case NETWORK_CHANNEL_STATE_CONNECTED:
      break;

    case NETWORK_CHANNEL_STATE_UNINITIALIZED:
    case NETWORK_CHANNEL_STATE_CLOSED:
      break;
    }
  }

  return NULL;
}

void CoapUdpIpChannel_ctor(CoapUdpIpChannel *self, const char *remote_address, int remote_protocol_family) {
  assert(self != NULL);
  assert(remote_address != NULL);

  // Initialize global coap server if not already done
  if (!_coap_is_globals_initialized) {
    _coap_is_globals_initialized = true;

    // Initialize coap server
    gcoap_register_listener(&_listener);

    // Create connection thread
    _connection_thread_pid =
        thread_create(_connection_thread_stack, sizeof(_connection_thread_stack), THREAD_PRIORITY_MAIN - 1, 0,
                      _CoapUdpIpChannel_connection_thread, NULL, "coap_connection_thread");
  }

  // Super fields
  self->super.expected_connect_duration = COAP_UDP_IP_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.type = NETWORK_CHANNEL_TYPE_COAP_UDP_IP;
  self->super.mode = NETWORK_CHANNEL_MODE_ASYNC;
  self->super.is_connected = CoapUdpIpChannel_is_connected;
  self->super.open_connection = CoapUdpIpChannel_open_connection;
  self->super.close_connection = CoapUdpIpChannel_close_connection;
  self->super.send_blocking = CoapUdpIpChannel_send_blocking;
  self->super.register_receive_callback = CoapUdpIpChannel_register_receive_callback;
  self->super.free = CoapUdpIpChannel_free;

  // Concrete fields
  self->receive_callback = NULL;
  self->encryption_layer = NULL;
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
  self->state_mutex = (mutex_t)MUTEX_INIT;

  // Convert host to udp socket
  if (inet_pton(remote_protocol_family, remote_address, self->remote.addr.ipv6) == 1) {
    self->remote.family = remote_protocol_family;
    self->remote.port = CONFIG_GCOAP_PORT;
  } else {
    COAP_UDP_IP_CHANNEL_ERR("Error parsing IP-Address");
  }
}
