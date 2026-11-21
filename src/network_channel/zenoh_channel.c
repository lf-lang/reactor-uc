#include "reactor-uc/network_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"

#ifdef NETWORK_CHANNEL_ZENOH

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if Z_FEATURE_MATCHING != 1 || Z_FEATURE_PUBLICATION != 1 || Z_FEATURE_SUBSCRIPTION != 1
#error "ZenohChannel needs zenoh-pico with Z_FEATURE_MATCHING, Z_FEATURE_PUBLICATION and Z_FEATURE_SUBSCRIPTION"
#endif

#define ZENOH_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "ZenohChannel: " fmt, ##__VA_ARGS__)
#define ZENOH_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "ZenohChannel: " fmt, ##__VA_ARGS__)
#define ZENOH_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "ZenohChannel: " fmt, ##__VA_ARGS__)

static ZenohChannelSession default_session;

ZenohChannelSession* ZenohChannel_default_session(void) { return &default_session; }

void ZenohChannelSession_configure(ZenohChannelSession* self, const char* mode, const char* connect, const char* listen,
                                   int32_t connect_timeout_ms) {
  if (self->session != NULL) {
    return;
  }
  self->mode = mode;
  self->connect = connect;
  self->listen = listen;
  self->connect_timeout_ms = connect_timeout_ms;
  self->is_configured = true;
}

/** Adds each locator of a comma-separated list to the configuration. */
static lf_ret_t ZenohChannelSession_insert_locators(z_owned_config_t* config, uint8_t key, const char* locators) {
  const char* start = locators;

  while (*start != '\0') {
    const char* end = strchr(start, ',');
    if (end == NULL) {
      end = start + strlen(start);
    }
    const char* first = start;
    const char* last = end;
    while (first < last && (*first == ' ' || *first == '\t')) {
      first++;
    }
    while (last > first && (last[-1] == ' ' || last[-1] == '\t')) {
      last--;
    }
    if (last > first) {
      char locator[ZENOH_CHANNEL_LOCATORSIZE];
      size_t length = (size_t)(last - first);

      if (length >= sizeof(locator)) {
        ZENOH_CHANNEL_ERR("A locator in \"%s\" is longer than %zu characters", locators, sizeof(locator) - 1);
        return LF_ERR;
      }
      memcpy(locator, first, length);
      locator[length] = '\0';
      if (zp_config_insert(z_loan_mut(*config), key, locator) != Z_OK) {
        ZENOH_CHANNEL_ERR("Could not add the locator %s", locator);
        return LF_ERR;
      }
    }
    start = (*end == ',') ? end + 1 : end;
  }
  return LF_OK;
}

lf_ret_t ZenohChannelSession_open(ZenohChannelSession* self, const char* mode, const char* connect, const char* listen,
                                  int32_t connect_timeout_ms) {
  z_owned_config_t config;

  z_config_default(&config);
  if (mode != NULL) {
    zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, mode);
  }
  if (connect != NULL && ZenohChannelSession_insert_locators(&config, Z_CONFIG_CONNECT_KEY, connect) != LF_OK) {
    z_drop(z_move(config));
    return LF_ERR;
  }
  if (listen != NULL) {
    zp_config_insert(z_loan_mut(config), Z_CONFIG_LISTEN_KEY, listen);
  }
  if (connect_timeout_ms != 0) {
#ifdef Z_FEATURE_UNSTABLE_API
    char timeout[16];
    int written = snprintf(timeout, sizeof(timeout), "%" PRId32, connect_timeout_ms);

    if (written <= 0 || (size_t)written >= sizeof(timeout)) {
      ZENOH_CHANNEL_ERR("Invalid connect timeout");
      z_drop(z_move(config));
      return LF_ERR;
    }
    zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_TIMEOUT_KEY, timeout);
#else
    ZENOH_CHANNEL_ERR("A connect timeout needs zenoh-pico built with Z_FEATURE_UNSTABLE_API");
    z_drop(z_move(config));
    return LF_ERR;
#endif
  }
  if (z_open(&self->owned, z_move(config), NULL) != Z_OK) {
    ZENOH_CHANNEL_DEBUG("Could not open the Zenoh session");
    self->session = NULL;
    return LF_ERR;
  }
  self->session = z_loan(self->owned);
  self->is_owned = true;
  return LF_OK;
}

void ZenohChannelSession_borrow(ZenohChannelSession* self, const z_loaned_session_t* session) {
  self->session = session;
  self->is_owned = false;
}

void ZenohChannelSession_close(ZenohChannelSession* self) {
  if (self->is_owned && self->session != NULL) {
    z_drop(z_move(self->owned));
  }
  self->session = NULL;
  self->is_owned = false;
}

static lf_ret_t ZenohChannel_try_open(ZenohChannel* self);

/**
 * The channel is connected when the other side's subscriber is known to this session. The other side applies the same
 * rule to its own publisher, so a message is only sent to a federate that is there to receive it.
 */
static bool ZenohChannel_is_connected(NetworkChannel* untyped_self) {
  ZenohChannel* self = (ZenohChannel*)untyped_self;
  z_matching_status_t status = {.matching = false};

  if (!self->is_open && (!self->wants_open || ZenohChannel_try_open(self) != LF_OK)) {
    return false;
  }
  if (z_publisher_get_matching_status(z_loan(self->publisher), &status) != Z_OK) {
    return false;
  }
  return status.matching;
}

/** Runs in the read task of the Zenoh session. */
static void ZenohChannel_sample_handler(z_loaned_sample_t* sample, void* arg) {
  ZenohChannel* self = (ZenohChannel*)arg;
  const z_loaned_bytes_t* payload = z_sample_payload(sample);
  size_t length = z_bytes_len(payload);

  if (length > sizeof(self->read_buffer)) {
    ZENOH_CHANNEL_ERR("Received %zu bytes, more than the buffer holds", length);
    return;
  }
  z_bytes_reader_t reader = z_bytes_get_reader(payload);
  if (z_bytes_reader_read(&reader, self->read_buffer, length) != length) {
    ZENOH_CHANNEL_ERR("Could not read the payload");
    return;
  }
  if (deserialize_from_protobuf(&self->output, self->read_buffer, length) < 0) {
    ZENOH_CHANNEL_ERR("Could not decode protobuf");
    return;
  }
  ZENOH_CHANNEL_DEBUG("Received msg %d", self->output.which_message);
  if (self->receive_callback != NULL) {
    self->receive_callback(self->federated_connection, &self->output);
  }
}

/**
 * Declares the subscriber and the publisher, after opening the session if the channel is the one to do that. Fails
 * while the session cannot be opened, for example because the network is not up yet; is_connected tries again.
 */
static lf_ret_t ZenohChannel_try_open(ZenohChannel* self) {
  z_view_keyexpr_t publish_key;
  z_view_keyexpr_t subscribe_key;
  z_owned_closure_sample_t callback;
  z_publisher_options_t options;
  bool opened_session = false;

  if (self->session->session == NULL) {
    if (ZenohChannelSession_open(self->session, self->session->mode, self->session->connect, self->session->listen,
                                 self->session->connect_timeout_ms) != LF_OK) {
      return LF_ERR;
    }
    opened_session = true;
  }
  z_view_keyexpr_from_str(&publish_key, self->publish_key);
  z_view_keyexpr_from_str(&subscribe_key, self->subscribe_key);

  z_closure(&callback, ZenohChannel_sample_handler, NULL, self);
  if (z_declare_subscriber(self->session->session, &self->subscriber, z_loan(subscribe_key), z_move(callback), NULL) !=
      Z_OK) {
    ZENOH_CHANNEL_ERR("Could not declare the subscriber on %s", self->subscribe_key);
    return LF_ERR;
  }

  // A federate must not lose a message because the link is congested.
  z_publisher_options_default(&options);
  options.congestion_control = Z_CONGESTION_CONTROL_BLOCK;
  if (z_declare_publisher(self->session->session, &self->publisher, z_loan(publish_key), &options) != Z_OK) {
    ZENOH_CHANNEL_ERR("Could not declare the publisher on %s", self->publish_key);
    z_drop(z_move(self->subscriber));
    return LF_ERR;
  }

  if (self->session->is_configured && (opened_session || self->session->open_channels > 0)) {
    self->session->open_channels++;
    self->counts_as_open_channel = true;
  }
  self->is_open = true;
  ZENOH_CHANNEL_DEBUG("Open: publishing on %s", self->publish_key);
  return LF_OK;
}

static lf_ret_t ZenohChannel_open_connection(NetworkChannel* untyped_self) {
  ZenohChannel* self = (ZenohChannel*)untyped_self;
  z_view_keyexpr_t key;

  if (self->is_open) {
    return LF_OK;
  }
  if (self->session == NULL || (self->session->session == NULL && !self->session->is_configured)) {
    ZENOH_CHANNEL_ERR("No Zenoh session: open, borrow or configure one first");
    return LF_ERR;
  }
  if (z_view_keyexpr_from_str(&key, self->publish_key) != Z_OK ||
      z_view_keyexpr_from_str(&key, self->subscribe_key) != Z_OK) {
    ZENOH_CHANNEL_ERR("Invalid key %s or %s", self->publish_key, self->subscribe_key);
    return LF_ERR;
  }
  self->wants_open = true;
  // Not being able to reach the network yet is no error: is_connected tries again.
  (void)ZenohChannel_try_open(self);
  return LF_OK;
}

static void ZenohChannel_close_connection(NetworkChannel* untyped_self) {
  ZenohChannel* self = (ZenohChannel*)untyped_self;

  self->wants_open = false;
  if (self->is_open) {
    self->is_open = false;
    z_drop(z_move(self->publisher));
    z_drop(z_move(self->subscriber));
    // The session the channels opened themselves goes with the last of them.
    if (self->counts_as_open_channel) {
      self->counts_as_open_channel = false;
      if (--self->session->open_channels == 0) {
        ZenohChannelSession_close(self->session);
      }
    }
  }
}

static lf_ret_t ZenohChannel_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  ZenohChannel* self = (ZenohChannel*)untyped_self;
  lf_ret_t ret = LF_ERR;

  if (!ZenohChannel_is_connected(untyped_self)) {
    return LF_ERR;
  }

  // The runtime and the session's read task (a reply sent from the receive callback) both send.
  z_mutex_lock(z_loan_mut(self->mutex));
  int message_size = serialize_to_protobuf(message, self->write_buffer, sizeof(self->write_buffer));
  if (message_size < 0) {
    ZENOH_CHANNEL_ERR("Could not encode protobuf");
  } else {
    z_owned_bytes_t payload;

    if (z_bytes_copy_from_buf(&payload, self->write_buffer, (size_t)message_size) != Z_OK) {
      ZENOH_CHANNEL_ERR("Could not allocate the payload");
    } else if (z_publisher_put(z_loan(self->publisher), z_move(payload), NULL) != Z_OK) {
      ZENOH_CHANNEL_ERR("Put failed");
    } else {
      ZENOH_CHANNEL_DEBUG("Sent msg %d, %d bytes", message->which_message, message_size);
      ret = LF_OK;
    }
  }
  z_mutex_unlock(z_loan_mut(self->mutex));
  return ret;
}

static void ZenohChannel_register_receive_callback(NetworkChannel* untyped_self,
                                                   void (*receive_callback)(FederatedConnectionBundle* conn,
                                                                            const FederateMessage* message),
                                                   FederatedConnectionBundle* conn) {
  ZenohChannel* self = (ZenohChannel*)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

static void ZenohChannel_free(NetworkChannel* untyped_self) {
  ZenohChannel* self = (ZenohChannel*)untyped_self;

  ZenohChannel_close_connection(untyped_self);
  z_drop(z_move(self->mutex));
}

void ZenohChannel_ctor(ZenohChannel* self, ZenohChannelSession* session, const char* prefix, const char* local,
                       const char* remote) {
  assert(self != NULL);
  assert(session != NULL);
  assert(prefix != NULL && local != NULL && remote != NULL);

  self->session = session;
  self->is_open = false;
  self->wants_open = false;
  self->counts_as_open_channel = false;
  self->receive_callback = NULL;
  self->federated_connection = NULL;

  int written = snprintf(self->publish_key, sizeof(self->publish_key), "%s/%s/%s", prefix, local, remote);
  validate(written > 0 && (size_t)written < sizeof(self->publish_key));
  written = snprintf(self->subscribe_key, sizeof(self->subscribe_key), "%s/%s/%s", prefix, remote, local);
  validate(written > 0 && (size_t)written < sizeof(self->subscribe_key));
  z_mutex_init(&self->mutex);

  self->super.is_connected = ZenohChannel_is_connected;
  self->super.open_connection = ZenohChannel_open_connection;
  self->super.close_connection = ZenohChannel_close_connection;
  self->super.send_blocking = ZenohChannel_send_blocking;
  self->super.register_receive_callback = ZenohChannel_register_receive_callback;
  self->super.free = ZenohChannel_free;
  self->super.expected_connect_duration = ZENOH_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.type = NETWORK_CHANNEL_TYPE_ZENOH;
  self->super.mode = NETWORK_CHANNEL_MODE_ASYNC;
}

#endif // NETWORK_CHANNEL_ZENOH
