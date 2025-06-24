#include "reactor-uc/platform/posix/coap_udp_ip_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/serialization.h"

#include <coap3/coap.h>
#include <pthread.h>

#define COAP_UDP_IP_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)
#define COAP_UDP_IP_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "CoapUdpIpChannel: " fmt, ##__VA_ARGS__)

static coap_context_t *_coap_context = NULL;
static pthread_t _connection_thread = 0;
static bool _coap_is_globals_initialized = false;
static pthread_mutex_t _global_mutex = PTHREAD_MUTEX_INITIALIZER;

static void _CoapUdpIpChannel_update_state(CoapUdpIpChannel *self, NetworkChannelState new_state) {
  COAP_UDP_IP_CHANNEL_DEBUG("Update state: %s => %s", NetworkChannel_state_to_string(self->state),
                            NetworkChannel_state_to_string(new_state));

  // Store old state
  NetworkChannelState old_state;

  // Update the state of the channel to its new state
  pthread_mutex_lock(&self->state_mutex);
  old_state = self->state;
  self->state = new_state;
  pthread_mutex_unlock(&self->state_mutex);

  // Inform runtime about new state if it changed from or to NETWORK_CHANNEL_STATE_CONNECTED
  if ((old_state == NETWORK_CHANNEL_STATE_CONNECTED && new_state != NETWORK_CHANNEL_STATE_CONNECTED) ||
      (old_state != NETWORK_CHANNEL_STATE_CONNECTED && new_state == NETWORK_CHANNEL_STATE_CONNECTED)) {
    _lf_environment->platform->notify(_lf_environment->platform);
  }

  // Signal connection thread to evaluate new state
  pthread_cond_signal(&self->state_cond);
}

static void _CoapUdpIpChannel_update_state_if_not(CoapUdpIpChannel *self, NetworkChannelState new_state,
                                                  NetworkChannelState if_not) {
  // Update the state of the channel itself
  pthread_mutex_lock(&self->state_mutex);
  if (self->state != if_not) {
    COAP_UDP_IP_CHANNEL_DEBUG("Update state: %s => %s", NetworkChannel_state_to_string(self->state),
                              NetworkChannel_state_to_string(new_state));
    self->state = new_state;
  }
  pthread_mutex_unlock(&self->state_mutex);

  // Inform runtime about new state
  _lf_environment->platform->notify(_lf_environment->platform);
}

static NetworkChannelState _CoapUdpIpChannel_get_state(CoapUdpIpChannel *self) {
  NetworkChannelState state;

  pthread_mutex_lock(&self->state_mutex);
  state = self->state;
  pthread_mutex_unlock(&self->state_mutex);

  return state;
}

static CoapUdpIpChannel *_CoapUdpIpChannel_get_coap_channel_by_session(coap_session_t *session) {
  CoapUdpIpChannel *channel;
  FederatedEnvironment *env = (FederatedEnvironment *)_lf_environment;

  // Get the remote session address
  coap_address_t remote_addr;
  coap_address_copy(&remote_addr, coap_session_get_addr_remote(session));
  // Set incoming port to COAP_DEFAULT_PORT to ignore it in the address comparison
  coap_address_set_port(&remote_addr, COAP_DEFAULT_PORT);

  for (size_t i = 0; i < env->net_bundles_size; i++) {
    if (env->net_bundles[i]->net_channel->type == NETWORK_CHANNEL_TYPE_COAP_UDP_IP) {
      channel = (CoapUdpIpChannel *)env->net_bundles[i]->net_channel;

      if (coap_address_equals(coap_session_get_addr_remote(channel->session), &remote_addr)) {
        return channel;
      }
    }
  }

  // Debug print address
  char addr_str[INET6_ADDRSTRLEN];
  coap_print_addr(&remote_addr, (unsigned char *)addr_str, sizeof(addr_str));
  COAP_UDP_IP_CHANNEL_ERR("Channel not found by session (addr=%s)", addr_str);
  return NULL;
}

static coap_response_t _CoapUdpIpChannel_client_response_handler(coap_session_t *session, const coap_pdu_t *sent,
                                                                 const coap_pdu_t *received, const coap_mid_t id) {
  (void)sent;
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_session(session);
  if (self == NULL) {
    return COAP_RESPONSE_FAIL;
  }

  // Verify this is the NACK that is expected (messages are not in order guaranteed)
  if (id != self->last_request_mid) {
    COAP_UDP_IP_CHANNEL_WARN("Received response for unexpected MID: %d (expected: %d)", id, self->last_request_mid);
    return COAP_RESPONSE_OK; // Ignore out-of-order responses
  }

  coap_pdu_code_t code = coap_pdu_get_code(received);
  bool success = (COAP_RESPONSE_CLASS(code) == 2);

  switch (self->last_request_type) {
  case COAP_REQUEST_TYPE_CONNECT:
    if (success) {
      _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTED);
    } else {
      COAP_UDP_IP_CHANNEL_ERR("CONNECTION REJECTED => Try to connect again");
      _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
    }
    break;

  case COAP_REQUEST_TYPE_MESSAGE:
    pthread_mutex_lock(&self->send_mutex);
    if (!success) {
      COAP_UDP_IP_CHANNEL_ERR("MESSAGE REJECTED");
      _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
    }
    pthread_cond_signal(&self->send_cond);
    pthread_mutex_unlock(&self->send_mutex);
    break;

  case COAP_REQUEST_TYPE_DISCONNECT:
    break;

  default:
    COAP_UDP_IP_CHANNEL_WARN("Received response for unknown request type: %d", self->last_request_type);
    break;
  }

  // Clear the last request info
  self->last_request_type = COAP_REQUEST_TYPE_NONE;
  self->last_request_mid = COAP_INVALID_MID;

  return COAP_RESPONSE_OK;
}

static void _CoapUdpIpChannel_client_nack_handler(coap_session_t *session, const coap_pdu_t *sent,
                                                  const coap_nack_reason_t reason, const coap_mid_t mid) {
  (void)sent;
  (void)reason;
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_session(session);
  if (self == NULL) {
    return;
  }

  // Verify this is the NACK that is expected (messages are not in order guaranteed)
  if (mid != self->last_request_mid) {
    COAP_UDP_IP_CHANNEL_WARN("Received NACK for unexpected MID: %d (expected: %d)", mid, self->last_request_mid);
    return;
  }

  switch (self->last_request_type) {
  case COAP_REQUEST_TYPE_CONNECT:
    COAP_UDP_IP_CHANNEL_ERR("TIMEOUT => Try to connect again");
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CONNECTION_FAILED);
    break;

  case COAP_REQUEST_TYPE_MESSAGE:
    COAP_UDP_IP_CHANNEL_ERR("MESSAGE TIMEOUT");
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_LOST_CONNECTION);
    pthread_mutex_lock(&self->send_mutex);
    pthread_cond_signal(&self->send_cond);
    pthread_mutex_unlock(&self->send_mutex);
    break;

  case COAP_REQUEST_TYPE_DISCONNECT:
    break;

  default:
    COAP_UDP_IP_CHANNEL_WARN("Received NACK for unknown request type: %d", self->last_request_type);
    break;
  }

  // Clear the last request info
  self->last_request_type = COAP_REQUEST_TYPE_NONE;
  self->last_request_mid = COAP_INVALID_MID;
}

static void _CoapUdpIpChannel_server_connect_handler(coap_resource_t *resource, coap_session_t *session,
                                                     const coap_pdu_t *request, const coap_string_t *query,
                                                     coap_pdu_t *response) {
  (void)response;
  (void)request;
  (void)session;
  (void)resource;
  (void)query;
  COAP_UDP_IP_CHANNEL_DEBUG("Server connect handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_session(session);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server connect handler: Client has unknown IP address");
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_UNAUTHORIZED);
    return;
  }

  // Error => return 503 (service unavailable)
  if (_CoapUdpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CLOSED) {
    COAP_UDP_IP_CHANNEL_ERR("Server connect handler: Channel is closed");
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_SERVICE_UNAVAILABLE);
    return;
  }

  // Success => return 204 (no content)
  coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
  return;
}

static void _CoapUdpIpChannel_server_disconnect_handler(coap_resource_t *resource, coap_session_t *session,
                                                        const coap_pdu_t *request, const coap_string_t *query,
                                                        coap_pdu_t *response) {
  (void)resource;
  (void)query;
  (void)request;
  COAP_UDP_IP_CHANNEL_DEBUG("Server disconnect handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_session(session);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server disconnect handler: Client has unknown IP address");
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_UNAUTHORIZED);
    return;
  }

  // Update state because it does not make sense to send data to a closed connection.
  if (_CoapUdpIpChannel_get_state(self) != NETWORK_CHANNEL_STATE_UNINITIALIZED &&
      _CoapUdpIpChannel_get_state(self) != NETWORK_CHANNEL_STATE_CLOSED) {
    _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CLOSED);
  }

  // Success => return 204 (no content)
  coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
  return;
}

static void _CoapUdpIpChannel_server_message_handler(coap_resource_t *resource, coap_session_t *session,
                                                     const coap_pdu_t *request, const coap_string_t *query,
                                                     coap_pdu_t *response) {
  (void)resource;
  (void)query;
  COAP_UDP_IP_CHANNEL_DEBUG("Server message handler");
  CoapUdpIpChannel *self = _CoapUdpIpChannel_get_coap_channel_by_session(session);

  // Error => return 401 (unauthorized)
  if (self == NULL) {
    COAP_UDP_IP_CHANNEL_ERR("Server message handler: Client has unknown IP address");
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_UNAUTHORIZED);
    return;
  }

  // Get payload from request
  const uint8_t *payload;
  size_t payload_len;
  if (coap_get_data(request, &payload_len, &payload) == 0) {
    COAP_UDP_IP_CHANNEL_ERR("Server message handler: No payload in request");
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
    return;
  }

  // Deserialize received message
  deserialize_from_protobuf(&self->output, payload, payload_len);
  COAP_UDP_IP_CHANNEL_DEBUG("Server message handler: Server received message");

  // Call registered receive callback to inform runtime about the new message
  if (self->receive_callback) {
    self->receive_callback(self->federated_connection, &self->output);
  }

  // Respond to the other federate
  coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
  return;
}

static bool _CoapUdpIpChannel_send_coap_message(CoapUdpIpChannel *self, const char *path,
                                                const FederateMessage *message) {
  if (!self->session) {
    COAP_UDP_IP_CHANNEL_ERR("No session available");
    return false;
  }

  coap_pdu_t *pdu = coap_new_pdu(COAP_MESSAGE_CON, COAP_REQUEST_CODE_POST, self->session);
  if (!pdu) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to create PDU");
    return false;
  }

  // Add URI path
  coap_add_option(pdu, COAP_OPTION_URI_PATH, strlen(path), (const uint8_t *)path);

  // Add payload if message is provided
  if (message) {
    uint8_t payload_buffer[COAP_UDP_IP_CHANNEL_BUFFERSIZE];
    int payload_len = serialize_to_protobuf(message, payload_buffer, sizeof(payload_buffer));

    if (payload_len < 0) {
      COAP_UDP_IP_CHANNEL_ERR("Could not encode protobuf");
      coap_delete_pdu(pdu);
      return false;
    }

    if ((unsigned long)payload_len > sizeof(payload_buffer)) {
      COAP_UDP_IP_CHANNEL_ERR("Payload too large (%d > %zu)", payload_len, sizeof(payload_buffer));
      coap_delete_pdu(pdu);
      return false;
    }

    uint8_t content_format = COAP_MEDIATYPE_APPLICATION_OCTET_STREAM;
    coap_add_option(pdu, COAP_OPTION_CONTENT_FORMAT, 1, &content_format);
    coap_add_data(pdu, payload_len, payload_buffer);
  }

  coap_mid_t mid = coap_send(self->session, pdu);
  if (mid == COAP_INVALID_MID) {
    COAP_UDP_IP_CHANNEL_ERR("Failed to send CoAP message");
    return false;
  }

  // Track this request for response handling
  if (strcmp(path, "connect") == 0) {
    self->last_request_type = COAP_REQUEST_TYPE_CONNECT;
  } else if (strcmp(path, "message") == 0) {
    self->last_request_type = COAP_REQUEST_TYPE_MESSAGE;
  } else if (strcmp(path, "disconnect") == 0) {
    self->last_request_type = COAP_REQUEST_TYPE_DISCONNECT;
  } else {
    self->last_request_type = COAP_REQUEST_TYPE_NONE;
  }
  self->last_request_mid = mid;

  COAP_UDP_IP_CHANNEL_DEBUG("CoAP Message sent (MID: %d, Type: %d)", mid, self->last_request_type);
  return true;
}

static lf_ret_t _CoapUdpIpChannel_client_send_connect_message(CoapUdpIpChannel *self) {
  if (!_CoapUdpIpChannel_send_coap_message(self, "connect", NULL)) {
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

  _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_OPEN);
  return LF_OK;
}

static void CoapUdpIpChannel_close_connection(NetworkChannel *untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Close connection");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Immediately close the channel
  _CoapUdpIpChannel_update_state(self, NETWORK_CHANNEL_STATE_CLOSED);

  // Inform the other federate that the channel is closed
  if (self->session) {
    _CoapUdpIpChannel_send_coap_message(self, "disconnect", NULL);
  }
}

static lf_ret_t CoapUdpIpChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  COAP_UDP_IP_CHANNEL_DEBUG("Send blocking");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Send message
  pthread_mutex_lock(&self->send_mutex);
  pthread_mutex_unlock(&self->send_mutex);

  if (_CoapUdpIpChannel_send_coap_message(self, "message", message)) {
    // Wait until the response handler confirms the ack or times out
    pthread_mutex_lock(&self->send_mutex);
    pthread_cond_wait(&self->send_cond, &self->send_mutex);
    pthread_mutex_unlock(&self->send_mutex);

    if (_CoapUdpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED) {
      return LF_OK;
    }
  }

  return LF_ERR;
}

static void CoapUdpIpChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                       void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                                const FederateMessage *msg),
                                                       FederatedConnectionBundle *conn) {
  COAP_UDP_IP_CHANNEL_INFO("Register receive callback");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

static void CoapUdpIpChannel_free(NetworkChannel *untyped_self) {
  COAP_UDP_IP_CHANNEL_DEBUG("Free");
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;

  // Close session if active
  if (self->session) {
    coap_session_release(self->session);
    self->session = NULL;
  }

  // Clean up mutexes and condition variables
  pthread_mutex_destroy(&self->state_mutex);
  pthread_cond_destroy(&self->state_cond);
  pthread_mutex_destroy(&self->send_mutex);
  pthread_cond_destroy(&self->send_cond);
}

static bool CoapUdpIpChannel_is_connected(NetworkChannel *untyped_self) {
  CoapUdpIpChannel *self = (CoapUdpIpChannel *)untyped_self;
  return _CoapUdpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED;
}

void *_CoapUdpIpChannel_connection_thread(void *arg) {
  (void)arg;
  COAP_UDP_IP_CHANNEL_DEBUG("Start connection thread");

  while (true) {
    // Process CoAP events
    if (_coap_context) {
      coap_io_process(_coap_context, 1000); // 1 second timeout
    }

    // Check all channels for state changes
    FederatedEnvironment *env = (FederatedEnvironment *)_lf_environment;
    for (size_t i = 0; i < env->net_bundles_size; i++) {
      if (env->net_bundles[i]->net_channel->type == NETWORK_CHANNEL_TYPE_COAP_UDP_IP) {
        CoapUdpIpChannel *self = (CoapUdpIpChannel *)env->net_bundles[i]->net_channel;

        NetworkChannelState state = _CoapUdpIpChannel_get_state(self);

        switch (state) {
        case NETWORK_CHANNEL_STATE_OPEN:
          /* try to connect */
          _CoapUdpIpChannel_client_send_connect_message(self);
          break;

        case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS:
          /* nothing to do */
          break;

        case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
        case NETWORK_CHANNEL_STATE_CONNECTION_FAILED:
          /* try to reconnect */
          _CoapUdpIpChannel_client_send_connect_message(self);
          break;

        case NETWORK_CHANNEL_STATE_CONNECTED:
        case NETWORK_CHANNEL_STATE_UNINITIALIZED:
        case NETWORK_CHANNEL_STATE_CLOSED:
          break;
        }
      }
    }
  }

  return NULL;
}

void CoapUdpIpChannel_ctor(CoapUdpIpChannel *self, const char *remote_host, int remote_protocol_family) {
  assert(self != NULL);
  assert(remote_host != NULL);

  // Initialize global coap context if not already done
  pthread_mutex_lock(&_global_mutex);
  if (!_coap_is_globals_initialized) {
    _coap_is_globals_initialized = true;

    // Initialize libcoap
    coap_startup();
    coap_set_log_level(COAP_LOG_DEBUG);

    // Create CoAP context and endpoints
    _coap_context = coap_new_context(NULL);
    if (!_coap_context) {
      COAP_UDP_IP_CHANNEL_ERR("Failed to create CoAP context");
      pthread_mutex_unlock(&_global_mutex);
      return;
    }

    // Set response and NACK handlers
    coap_register_response_handler(_coap_context, _CoapUdpIpChannel_client_response_handler);
    coap_register_nack_handler(_coap_context, _CoapUdpIpChannel_client_nack_handler);

    // Let libcoap handle multi-block payloads
    coap_context_set_block_mode(_coap_context, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

    // Create CoAP listening endpoint(s)
    int scheme_hint_bits = coap_get_available_scheme_hint_bits(0, 0, COAP_PROTO_UDP);
    coap_addr_info_t *info_list = coap_resolve_address_info(NULL, COAP_DEFAULT_PORT, 0, 0, 0, AF_UNSPEC,
                                                            scheme_hint_bits, COAP_RESOLVE_TYPE_LOCAL);

    bool endpoint_created = false;
    for (coap_addr_info_t *info = info_list; info != NULL; info = info->next) {
      coap_endpoint_t *ep = coap_new_endpoint(_coap_context, &info->addr, info->proto);
      if (ep) {
        endpoint_created = true;
        COAP_UDP_IP_CHANNEL_DEBUG("Created CoAP endpoint for protocol %u", info->proto);
      } else {
        COAP_UDP_IP_CHANNEL_WARN("Failed to create endpoint for protocol %u", info->proto);
      }
    }
    coap_free_address_info(info_list);

    if (!endpoint_created) {
      COAP_UDP_IP_CHANNEL_ERR("Failed to create any CoAP endpoints");
      coap_free_context(_coap_context);
      _coap_context = NULL;
      pthread_mutex_unlock(&_global_mutex);
      return;
    }

    // Create server resources
    coap_resource_t *connect_resource = coap_resource_init(coap_make_str_const("connect"), 0);
    coap_register_handler(connect_resource, COAP_REQUEST_POST, _CoapUdpIpChannel_server_connect_handler);
    coap_add_resource(_coap_context, connect_resource);

    coap_resource_t *disconnect_resource = coap_resource_init(coap_make_str_const("disconnect"), 0);
    coap_register_handler(disconnect_resource, COAP_REQUEST_POST, _CoapUdpIpChannel_server_disconnect_handler);
    coap_add_resource(_coap_context, disconnect_resource);

    coap_resource_t *message_resource = coap_resource_init(coap_make_str_const("message"), 0);
    coap_register_handler(message_resource, COAP_REQUEST_POST, _CoapUdpIpChannel_server_message_handler);
    coap_add_resource(_coap_context, message_resource);

    // Create connection thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, COAP_UDP_IP_CHANNEL_RECV_THREAD_STACK_SIZE);
    if (pthread_create(&_connection_thread, &attr, _CoapUdpIpChannel_connection_thread, NULL) != 0) {
      COAP_UDP_IP_CHANNEL_ERR("Failed to create connection thread");
    }
    pthread_attr_destroy(&attr);
  }
  pthread_mutex_unlock(&_global_mutex);

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
  self->session = NULL;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;

  // Initialize mutexes and condition variables
  pthread_mutex_init(&self->state_mutex, NULL);
  pthread_cond_init(&self->state_cond, NULL);
  pthread_mutex_init(&self->send_mutex, NULL);
  pthread_cond_init(&self->send_cond, NULL);

  // Convert host to coap address
  coap_str_const_t *host_str = coap_make_str_const(remote_host);
  int scheme_hint_bits = coap_get_available_scheme_hint_bits(0, 0, COAP_PROTO_UDP);
  coap_addr_info_t *addr_info =
      coap_resolve_address_info(host_str, COAP_DEFAULT_PORT, COAP_DEFAULT_PORT, 0, 0, remote_protocol_family,
                                scheme_hint_bits, COAP_RESOLVE_TYPE_REMOTE);
  if (addr_info) {
    // Create client session
    self->session = coap_new_client_session(_coap_context, NULL, &addr_info->addr, COAP_PROTO_UDP);
    if (!self->session) {
      COAP_UDP_IP_CHANNEL_ERR("Failed to create client session");
    }
    coap_free_address_info(addr_info);
  } else {
    COAP_UDP_IP_CHANNEL_ERR("Error resolving remote address: %s", remote_host);
  }
}
