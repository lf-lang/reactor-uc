/**
 * @file ble_channel.c
 * @brief BLE NetworkChannel for federated Reactor-UC on Zephyr/Nordic.
 *
 * A POLLED BLE channel: BLE callbacks run in the cooperative Bluetooth RX 
 * thread and only stage framed bytes into `receive_buffer`, then wake the 
 * reactor loop via platform->notify(); poll() on the main thread de-frames, 
 * deserializes and dispatches. See ble_channel.h for the wire/role design.
 *
 * Zephyr's connection/scan callbacks and the GATT service are process-global, so
 * a small registry `g_channels[]` routes each event to the owning BleChannel:
 * per-connection events match on `bt_conn *` (or peer address for a just-created
 * central link). GATT-client events recover `self` via CONTAINER_OF on the
 * channel's own param block. Scanning is shared across all waiting centrals. A
 * central board may hold one channel per upstream peripheral. A board hosts at
 * most one peripheral channel (legacy advertising exposes a single name).
 *
 * Connection parameters (interval/latency/timeout) are applied ONCE at
 * bt_conn_le_create(). We deliberately do NOT re-request them after connecting:
 * that LL procedure, overlapping GATT discovery, wedges the nRF54L controller. 
 * The peripheral accepts them (le_param_req) and both sides record the 
 * negotiated values (le_param_updated).
 */

#include "reactor-uc/platform/zephyr/ble_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/environment.h"

#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <string.h>

#define BLE_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "BleChannel: " fmt, ##__VA_ARGS__)
#define BLE_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "BleChannel: " fmt, ##__VA_ARGS__)
#define BLE_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "BleChannel: " fmt, ##__VA_ARGS__)
#define BLE_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "BleChannel: " fmt, ##__VA_ARGS__)

// On-wire framing (same as UartPolledChannel): PREFIX | protobuf | POSTFIX,
// which tolerates BLE fragmentation across PDUs / notifications.
#define BLE_MESSAGE_PREFIX {0xAA, 0xAA, 0xAA, 0xAA, 0xAA}
#define BLE_MESSAGE_POSTFIX {0xBB, 0xBB, 0xBB, 0xBB, 0xBD}
#define BLE_MINIMUM_MESSAGE_SIZE 10
#define BLE_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(5000)
#define BLE_DEFAULT_TX_PAYLOAD 20 // ATT_MTU(23) - 3, before any MTU exchange

// One channel per simultaneous link, bounded by the controller's connection count.
// Multiple channels may coexist on a single board, but currently only a single 
// one has been tested yet.
#ifndef CONFIG_BT_MAX_CONN
#define CONFIG_BT_MAX_CONN 1
#endif
#define BLE_MAX_CHANNELS CONFIG_BT_MAX_CONN

// Nordic UART Service UUIDs, reused so the peer is also pokeable from the nRF
// Connect mobile app: service 6E400001-…, RX (write) …0002, TX (notify) …0003.
static struct bt_uuid_128 ble_svc_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e));
static struct bt_uuid_128 ble_rx_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e));
static struct bt_uuid_128 ble_tx_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x6e400003, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e));
static struct bt_uuid_16 ble_ccc_uuid = BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);

// Process-global registry. The GATT service and connection/scan callbacks are
// shared, so events are routed back to the owning channel through this table.
static BleChannel* g_channels[BLE_MAX_CHANNELS];
static size_t g_num_channels = 0;
static bool g_bt_ready = false;
static bool g_scanning = false;

static int ble_start_advertising(BleChannel* self);
static int ble_ensure_scanning(void);
static void ble_scan_cb(const bt_addr_le_t* addr, int8_t rssi, uint8_t type, struct net_buf_simple* ad);

static BleChannel* ble_channel_by_conn(struct bt_conn* conn) {
  for (size_t i = 0; i < g_num_channels; i++) {
    if (g_channels[i] != NULL && g_channels[i]->conn == conn) {
      return g_channels[i];
    }
  }
  return NULL;
}

// Opened and not yet closed.
static bool ble_channel_active(const BleChannel* ch) {
  return ch != NULL && ch->state != NETWORK_CHANNEL_STATE_CLOSED && ch->state != NETWORK_CHANNEL_STATE_UNINITIALIZED;
}

// A central link still seeking a peer (initial connect or post-drop retry).
static bool ble_central_needs_conn(const BleChannel* ch) {
  return ble_channel_active(ch) && ch->role == BLE_CHANNEL_ROLE_CENTRAL && ch->conn == NULL;
}

// Any central channel is still seeking a peer (initial connect or post-drop retry).
static bool ble_any_central_needs_conn(void) {
  for (size_t i = 0; i < g_num_channels; i++) {
    if (ble_central_needs_conn(g_channels[i])) {
      return true;
    }
  }
  return false;
}

// The peripheral channel awaiting an incoming connection (one per board).
static BleChannel* ble_waiting_peripheral(void) {
  for (size_t i = 0; i < g_num_channels; i++) {
    BleChannel* ch = g_channels[i];
    if (ble_channel_active(ch) && ch->role == BLE_CHANNEL_ROLE_PERIPHERAL && ch->conn == NULL) {
      return ch;
    }
  }
  return NULL;
}

// Wake the reactor loop. Safe from BT callback context (k_sem_give is non-blocking).
static void ble_wake_runtime(void) {
  if (_lf_environment != NULL && _lf_environment->platform != NULL) {
    _lf_environment->platform->notify(_lf_environment->platform);
  }
}

// Stage received bytes and wake the loop. Runs in the cooperative BT RX thread, so
// it guards the buffer with a spinlock, never a blocking k_mutex (which would stall
// all ATT processing).
static void ble_rx_append(BleChannel* self, const uint8_t* data, uint16_t len) {
  if (self == NULL) {
    return;
  }
  k_spinlock_key_t key = k_spin_lock(&self->rx_lock);
  bool overflow = self->receive_buffer_index + len > BLE_CHANNEL_BUFFERSIZE;
  if (!overflow) {
    memcpy(self->receive_buffer + self->receive_buffer_index, data, len);
    self->receive_buffer_index += len;
  }
  k_spin_unlock(&self->rx_lock, key);

  if (overflow) {
    BLE_CHANNEL_ERR("receive buffer overflow, dropping %u bytes", len);
  }
  ble_wake_runtime();
}

// Central wrote the RX characteristic (central -> peripheral data).
static ssize_t ble_rx_write_cb(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len,
                               uint16_t offset, uint8_t flags) {
  (void)attr;
  (void)offset;
  (void)flags;
  BLE_CHANNEL_DEBUG("RX write: %u bytes", len);
  BleChannel* self = ble_channel_by_conn(conn);
  if (self == NULL) {
    self = ble_waiting_peripheral(); // conn may not be assigned yet
  }
  ble_rx_append(self, (const uint8_t*)buf, len);
  return len;
}

// Central enabled/disabled notifications on our TX characteristic's CCC. The CCC
// write carries no bt_conn; with one peripheral channel per board it belongs to
// the peripheral link that just connected.
static void ble_ccc_changed_cb(const struct bt_gatt_attr* attr, uint16_t value) {
  (void)attr;
  bool enabled = (value == BT_GATT_CCC_NOTIFY);
  BLE_CHANNEL_DEBUG("CCC changed: notifications %s", enabled ? "enabled" : "disabled");
  for (size_t i = 0; i < g_num_channels; i++) {
    BleChannel* ch = g_channels[i];
    if (ch != NULL && ch->role == BLE_CHANNEL_ROLE_PERIPHERAL && ch->conn != NULL) {
      ch->subscribed = enabled;
      if (enabled) {
        ch->state = NETWORK_CHANNEL_STATE_CONNECTED;
      }
    }
  }
  if (enabled) {
    ble_wake_runtime(); // let the startup connect-loop re-check is_connected
  }
}

// GATT layout: [0] service [1] TX decl [2] TX value [3] CCC [4] RX decl [5] RX value.
// bt_gatt_notify() must target the TX VALUE attribute ([2]), passing the
// declaration notifies the wrong handle and the central never receives.
BT_GATT_SERVICE_DEFINE(ble_svc, BT_GATT_PRIMARY_SERVICE(&ble_svc_uuid),
                       BT_GATT_CHARACTERISTIC(&ble_tx_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL,
                                              NULL),
                       BT_GATT_CCC(ble_ccc_changed_cb, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(&ble_rx_uuid.uuid, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE, NULL, ble_rx_write_cb, NULL));
#define BLE_TX_NOTIFY_ATTR (&ble_svc.attrs[2])

static void ble_start_discovery(BleChannel* self, struct bt_conn* conn); // fwd decl

// Notification from the peripheral's TX characteristic (peripheral -> central data).
static uint8_t ble_notify_cb(struct bt_conn* conn, struct bt_gatt_subscribe_params* params, const void* data,
                             uint16_t length) {
  (void)conn;
  BleChannel* self = CONTAINER_OF(params, BleChannel, subscribe_params);
  if (data == NULL) { // unsubscribed
    params->value_handle = 0U;
    self->subscribed = false;
    return BT_GATT_ITER_STOP;
  }
  ble_rx_append(self, (const uint8_t*)data, length);
  return BT_GATT_ITER_CONTINUE;
}

static void ble_mtu_cb(struct bt_conn* conn, uint8_t err, struct bt_gatt_exchange_params* params) {
  BleChannel* self = CONTAINER_OF(params, BleChannel, mtu_params);
  if (err == 0) {
    uint16_t mtu = bt_gatt_get_mtu(conn);
    self->tx_payload = (mtu > 3) ? (mtu - 3) : BLE_DEFAULT_TX_PAYLOAD;
    BLE_CHANNEL_DEBUG("ATT MTU exchanged: %u (tx_payload=%u)", mtu, self->tx_payload);
  }
  // Discovery may start only after MTU exchange completes: one ATT op at a time.
  ble_start_discovery(self, conn);
}

// Issue the next discovery step (keeps exactly one ATT op in flight).
static void ble_discover_next(BleChannel* self, struct bt_conn* conn, const struct bt_uuid* uuid, uint16_t start_handle,
                              uint8_t type) {
  self->discover_params.uuid = uuid;
  self->discover_params.start_handle = start_handle;
  self->discover_params.type = type;
  int err = bt_gatt_discover(conn, &self->discover_params);
  if (err) {
    BLE_CHANNEL_ERR("bt_gatt_discover failed (%d)", err);
  }
}

// Walk the service one ATT op per callback: service -> TX char -> CCC -> RX char
// -> subscribe. Subscribe (a CCC write) is the terminal step, so no callback ever
// has two ATT operations in flight.
static uint8_t ble_discover_cb(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                               struct bt_gatt_discover_params* params) {
  BleChannel* self = CONTAINER_OF(params, BleChannel, discover_params);
  if (attr == NULL) {
    BLE_CHANNEL_WARN("Discovery ended before RX/TX resolved");
    return BT_GATT_ITER_STOP;
  }

  if (bt_uuid_cmp(params->uuid, &ble_svc_uuid.uuid) == 0) {
    // Service found -> find the TX (notify) characteristic.
    ble_discover_next(self, conn, &ble_tx_uuid.uuid, attr->handle + 1, BT_GATT_DISCOVER_CHARACTERISTIC);
  } else if (bt_uuid_cmp(params->uuid, &ble_tx_uuid.uuid) == 0) {
    // TX found -> remember its value handle, then find its CCC descriptor.
    self->peer_tx_handle = bt_gatt_attr_value_handle(attr);
    ble_discover_next(self, conn, &ble_ccc_uuid.uuid, attr->handle + 2, BT_GATT_DISCOVER_DESCRIPTOR);
  } else if (bt_uuid_cmp(params->uuid, &ble_ccc_uuid.uuid) == 0) {
    // CCC found -> stage the subscription, then find the RX characteristic. The
    // subscribe itself is deferred to the RX branch (one ATT op per callback).
    self->subscribe_params.notify = ble_notify_cb;
    self->subscribe_params.value = BT_GATT_CCC_NOTIFY;
    self->subscribe_params.value_handle = self->peer_tx_handle;
    self->subscribe_params.ccc_handle = attr->handle;
    ble_discover_next(self, conn, &ble_rx_uuid.uuid, attr->handle + 1, BT_GATT_DISCOVER_CHARACTERISTIC);
  } else if (bt_uuid_cmp(params->uuid, &ble_rx_uuid.uuid) == 0) {
    // RX found -> both handles known. Subscribe last: this CCC write is the
    // terminal ATT op and marks the central link CONNECTED.
    self->peer_rx_handle = bt_gatt_attr_value_handle(attr);
    self->discovered = true;
    int err = bt_gatt_subscribe(conn, &self->subscribe_params);
    if (err && err != -EALREADY) {
      BLE_CHANNEL_ERR("Subscribe failed (%d)", err);
    } else {
      self->subscribed = true;
      self->state = NETWORK_CHANNEL_STATE_CONNECTED;
      BLE_CHANNEL_INFO("Central link to '%s' connected (rx=%u tx=%u)", self->device_name, self->peer_rx_handle,
                       self->peer_tx_handle);
      ble_wake_runtime(); // let the startup connect-loop re-check is_connected
    }
  } else {
    BLE_CHANNEL_ERR("Unexpected discovery UUID");
  }
  return BT_GATT_ITER_STOP;
}

static void ble_start_discovery(BleChannel* self, struct bt_conn* conn) {
  self->discovered = false;
  self->subscribed = false;
  self->discover_params.func = ble_discover_cb;
  self->discover_params.end_handle = 0xffff; // BT_ATT_LAST_ATTRIBUTE_HANDLE
  ble_discover_next(self, conn, &ble_svc_uuid.uuid, 0x0001, BT_GATT_DISCOVER_PRIMARY);
}

struct ble_name_extract {
  char name[32];
  uint8_t len;
};

static bool ble_adv_name_parse(struct bt_data* data, void* user_data) {
  struct ble_name_extract* e = (struct ble_name_extract*)user_data;
  if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
    e->len = (uint8_t)MIN(data->data_len, (uint16_t)(sizeof(e->name) - 1));
    memcpy(e->name, data->data, e->len);
    e->name[e->len] = '\0';
    return false; // stop parsing
  }
  return true;
}

// The central channel (still seeking a peer) whose device_name matches this ad.
static BleChannel* ble_match_adv(const struct ble_name_extract* adv) {
  for (size_t i = 0; i < g_num_channels; i++) {
    BleChannel* ch = g_channels[i];
    if (ble_central_needs_conn(ch) && strlen(ch->device_name) == adv->len &&
        memcmp(ch->device_name, adv->name, adv->len) == 0) {
      return ch;
    }
  }
  return NULL;
}

static void ble_scan_cb(const bt_addr_le_t* addr, int8_t rssi, uint8_t type, struct net_buf_simple* ad) {
  (void)rssi;
  if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND && type != BT_GAP_ADV_TYPE_EXT_ADV) {
    return; // connectable advertising only
  }
  struct ble_name_extract adv = {.len = 0};
  bt_data_parse(ad, ble_adv_name_parse, &adv);
  if (adv.len == 0) {
    return;
  }
  BleChannel* self = ble_match_adv(&adv);
  if (self == NULL) {
    return;
  }

  BLE_CHANNEL_INFO("Found peer '%s', connecting", self->device_name);
  bt_le_scan_stop(); // a connection cannot be created while scanning
  g_scanning = false;

  // The `connected` callback matches this link back by peer address, so the test
  // boards must use fixed (public/static) addresses.
  memcpy(&self->peer_addr, addr, sizeof(self->peer_addr));
  struct bt_le_conn_param cp = {
      .interval_min = self->params.interval,
      .interval_max = self->params.interval,
      .latency = self->params.latency,
      .timeout = self->params.supervision_timeout,
  };
  struct bt_conn* conn = NULL;
  int err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, &cp, &conn);
  if (err) {
    BLE_CHANNEL_ERR("bt_conn_le_create failed (%d), resuming scan", err);
    if (ble_any_central_needs_conn()) {
      ble_ensure_scanning();
    }
  } else if (conn != NULL) {
    bt_conn_unref(conn); // the `connected` callback takes its own reference
  }
}

// Start the shared scanner if it is not already running (idempotent).
static int ble_ensure_scanning(void) {
  if (g_scanning) {
    return 0;
  }
  int err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, ble_scan_cb);
  if (err && err != -EALREADY) {
    BLE_CHANNEL_ERR("Scanning failed to start (%d)", err);
    return err;
  }
  g_scanning = true;
  BLE_CHANNEL_INFO("Scanning for peers");
  return 0;
}

static int ble_start_advertising(BleChannel* self) {
  // Connectable/undirected, name carried in the AD. The central matches on this 
  // name in ble_adv_name_parse().
  struct bt_data ble_ad[] = {
      BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
      BT_DATA(BT_DATA_NAME_COMPLETE, self->device_name, strlen(self->device_name)),
  };
  int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ble_ad, ARRAY_SIZE(ble_ad), NULL, 0);
  if (err && err != -EALREADY) {
    BLE_CHANNEL_ERR("Advertising failed to start (%d)", err);
    return err;
  }
  BLE_CHANNEL_INFO("Advertising as '%s'", self->device_name);
  return 0;
}

// Route a `connected` event: a central that initiated to this peer, else the
// waiting peripheral.
static BleChannel* ble_owner_of_new_conn(const bt_addr_le_t* peer) {
  for (size_t i = 0; i < g_num_channels; i++) {
    BleChannel* ch = g_channels[i];
    if (ble_central_needs_conn(ch) && bt_addr_le_cmp(&ch->peer_addr, peer) == 0) {
      return ch;
    }
  }
  return ble_waiting_peripheral();
}

static void ble_connected_cb(struct bt_conn* conn, uint8_t err) {
  BleChannel* self = ble_owner_of_new_conn(bt_conn_get_dst(conn));
  if (self == NULL) {
    return; // not one of ours
  }

  if (err) {
    BLE_CHANNEL_ERR("Connection failed (0x%02x)", err);
    self->state = NETWORK_CHANNEL_STATE_CONNECTION_FAILED;
    if (self->role == BLE_CHANNEL_ROLE_CENTRAL && ble_any_central_needs_conn()) {
      ble_ensure_scanning();
    }
    return;
  }

  BLE_CHANNEL_INFO("BLE link established (%s)", self->device_name);
  self->conn = bt_conn_ref(conn);
  self->tx_payload = BLE_DEFAULT_TX_PAYLOAD;

  if (self->role == BLE_CHANNEL_ROLE_CENTRAL) {
    // Params were set at bt_conn_le_create(), go straight to MTU exchange, then
    // chain discovery from ble_mtu_cb (one ATT op at a time). Do NOT re-request
    // params here. If MTU can't start, discover directly so we never stall.
    self->mtu_params.func = ble_mtu_cb;
    if (bt_gatt_exchange_mtu(conn, &self->mtu_params) != 0) {
      ble_start_discovery(self, conn);
    }
    if (ble_any_central_needs_conn()) {
      ble_ensure_scanning(); // other central links may still need a peer
    }
  }
  // A peripheral becomes CONNECTED only once the central enables notifications (CCC).
}

// Re-arm the link after a drop. Runs on the system workqueue, NOT in the
// disconnected callback: restarting connectable advertising from inside
// `disconnected` fails with -ENOMEM, because the just-dropped connection slot is
// not released until the callback returns (with CONFIG_BT_MAX_CONN=1 there is
// no free slot to advertise into). Deferring here lets the slot free first.
static void ble_recover_work_handler(struct k_work* work) {
  BleChannel* self = CONTAINER_OF(work, BleChannel, recover_work);
  if (self->conn != NULL || !ble_channel_active(self)) {
    return; // already reconnected, or closed
  }
  if (self->role == BLE_CHANNEL_ROLE_PERIPHERAL) {
    ble_start_advertising(self);
  } else {
    ble_ensure_scanning();
  }
}

static void ble_disconnected_cb(struct bt_conn* conn, uint8_t reason) {
  BleChannel* self = ble_channel_by_conn(conn);
  BLE_CHANNEL_WARN("BLE link lost (reason 0x%02x)", reason);
  if (self == NULL) {
    return;
  }
  bt_conn_unref(self->conn);
  self->conn = NULL;
  self->state = NETWORK_CHANNEL_STATE_LOST_CONNECTION;
  self->subscribed = false;
  self->discovered = false;
  // Re-arm off this callback: advertising/scanning from here can fail until 
  // the connection slot frees.
  k_work_submit(&self->recover_work);
}

// Record the negotiated parameters (the controlled CI/SL/timeout).
static void ble_le_param_updated_cb(struct bt_conn* conn, uint16_t interval, uint16_t latency, uint16_t timeout) {
  BleChannel* self = ble_channel_by_conn(conn);
  if (self == NULL) {
    return;
  }
  self->act_interval = interval;
  self->act_latency = latency;
  self->act_timeout = timeout;
  BLE_CHANNEL_INFO("conn params applied (%s): CI=%u (%u.%02u ms) latency=%u timeout=%u (%u ms)", self->device_name,
                   interval, (interval * 125) / 100, (interval * 125) % 100, latency, timeout, timeout * 10);
}

// Peripheral accepts the central's requested parameters.
static bool ble_le_param_req_cb(struct bt_conn* conn, struct bt_le_conn_param* param) {
  (void)conn;
  (void)param;
  return true;
}

BT_CONN_CB_DEFINE(ble_conn_callbacks) = {
    .connected = ble_connected_cb,
    .disconnected = ble_disconnected_cb,
    .le_param_req = ble_le_param_req_cb,
    .le_param_updated = ble_le_param_updated_cb,
};

// Transmit already-framed bytes, chunked to the negotiated PDU payload. Runs on the
// (preemptible) main thread, so bt_gatt_* blocks for buffers rather than failing.
// Treat a rare -ENOMEM/-EAGAIN as transient backpressure and retry.
static lf_ret_t ble_send_raw(BleChannel* self, const uint8_t* data, uint16_t len) {
  if (self->conn == NULL) {
    BLE_CHANNEL_ERR("send while disconnected");
    return LF_ERR;
  }
  uint16_t chunk = (self->tx_payload > 0) ? self->tx_payload : BLE_DEFAULT_TX_PAYLOAD;
  for (uint16_t off = 0; off < len;) {
    uint16_t n = MIN(chunk, (uint16_t)(len - off));
    int err = (self->role == BLE_CHANNEL_ROLE_CENTRAL)
                  ? bt_gatt_write_without_response(self->conn, self->peer_rx_handle, data + off, n, false)
                  : bt_gatt_notify(self->conn, BLE_TX_NOTIFY_ATTR, data + off, n);
    if (err == -ENOMEM || err == -EAGAIN) {
      k_sleep(K_MSEC(2)); // let the controller drain, then retry
      continue;
    }
    if (err) {
      BLE_CHANNEL_ERR("GATT send failed (%d)", err);
      return LF_ERR;
    }
    off += n;
  }
  return LF_OK;
}

static bool BleChannel_is_connected(NetworkChannel* untyped_self) {
  BleChannel* self = (BleChannel*)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t BleChannel_open_connection(NetworkChannel* untyped_self) {
  BleChannel* self = (BleChannel*)untyped_self;
  BLE_CHANNEL_DEBUG("Open connection to '%s' (%s)", self->device_name,
                    self->role == BLE_CHANNEL_ROLE_PERIPHERAL ? "peripheral" : "central");
  self->state = NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS;
  int err = (self->role == BLE_CHANNEL_ROLE_PERIPHERAL) ? ble_start_advertising(self) : ble_ensure_scanning();
  return err ? LF_ERR : LF_OK;
}

static void BleChannel_close_connection(NetworkChannel* untyped_self) {
  BleChannel* self = (BleChannel*)untyped_self;
  BLE_CHANNEL_DEBUG("Close connection");
  if (self->conn != NULL) {
    bt_conn_disconnect(self->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
  }
  self->state = NETWORK_CHANNEL_STATE_CLOSED;
}

static lf_ret_t BleChannel_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  BleChannel* self = (BleChannel*)untyped_self;
  unsigned char prefix[] = BLE_MESSAGE_PREFIX;
  unsigned char postfix[] = BLE_MESSAGE_POSTFIX;

  memcpy(self->send_buffer, prefix, sizeof(prefix));
  int message_size = serialize_to_protobuf(message, self->send_buffer + sizeof(prefix),
                                           BLE_CHANNEL_BUFFERSIZE - sizeof(prefix) - sizeof(postfix));
  if (message_size < 0) {
    BLE_CHANNEL_ERR("Serialization failed");
    return LF_ERR;
  }
  memcpy(self->send_buffer + sizeof(prefix) + message_size, postfix, sizeof(postfix));
  uint16_t total = (uint16_t)(sizeof(prefix) + message_size + sizeof(postfix));
  BLE_CHANNEL_DEBUG("Sending framed message of %u bytes", total);
  return ble_send_raw(self, self->send_buffer, total);
}

static void BleChannel_register_receive_callback(NetworkChannel* untyped_self,
                                                 void (*receive_callback)(FederatedConnectionBundle* conn,
                                                                          const FederateMessage* message),
                                                 FederatedConnectionBundle* bundle) {
  BleChannel* self = (BleChannel*)untyped_self;
  BLE_CHANNEL_DEBUG("Register receive callback");
  self->receive_callback = receive_callback;
  self->bundle = bundle;
}

// De-frame and dispatch buffered bytes.
static lf_ret_t BleChannel_poll(NetworkChannel* untyped_self) {
  BleChannel* self = (BleChannel*)untyped_self;
  unsigned char prefix[] = BLE_MESSAGE_PREFIX;
  unsigned char postfix[] = BLE_MESSAGE_POSTFIX;
  bool processed = false;

  while (self->receive_buffer_index > BLE_MINIMUM_MESSAGE_SIZE) {
    int start = -1;
    for (int i = 0; i <= (int)(self->receive_buffer_index - sizeof(prefix)); i++) {
      if (memcmp(prefix, &self->receive_buffer[i], sizeof(prefix)) == 0) {
        start = i;
        break;
      }
    }
    if (start == -1) {
      break;
    }
    int end = -1;
    for (int i = start; i <= (int)(self->receive_buffer_index - sizeof(postfix)); i++) {
      if (memcmp(postfix, &self->receive_buffer[i], sizeof(postfix)) == 0) {
        end = i;
        break;
      }
    }
    if (end == -1) {
      break;
    }
    int payload_start = start + (int)sizeof(prefix);

    // Deserialize the committed region [payload_start, end) outside the lock: it
    // sits below receive_buffer_index, which ble_rx_append() never writes into.
    // Only the index update + compaction memmove race the appender, so hold the
    // (IRQ-disabling) spinlock tightly around just those.
    int bytes_left =
        deserialize_from_protobuf(&self->output, self->receive_buffer + payload_start, end - payload_start);
    k_spinlock_key_t key = k_spin_lock(&self->rx_lock);
    int consumed = end + (int)sizeof(postfix);
    int old_index = (int)self->receive_buffer_index;
    self->receive_buffer_index -= consumed;
    memmove(self->receive_buffer, self->receive_buffer + consumed, old_index - consumed);
    k_spin_unlock(&self->rx_lock, key);

    if (bytes_left >= 0 && self->receive_callback != NULL) {
      self->receive_callback(self->bundle, &self->output);
      processed = true;
    }
  }
  // Scheduler drain-loop contract: RETRY == "processed one, poll again", EMPTY ==
  // "nothing left". 
  return processed ? LF_NETWORK_CHANNEL_RETRY : LF_NETWORK_CHANNEL_EMPTY;
}

static void BleChannel_free(NetworkChannel* untyped_self) {
  BleChannel* self = (BleChannel*)untyped_self;
  BLE_CHANNEL_DEBUG("Free");
  if (self->conn != NULL) {
    bt_conn_disconnect(self->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    bt_conn_unref(self->conn);
    self->conn = NULL;
  }
  for (size_t i = 0; i < g_num_channels; i++) {
    if (g_channels[i] == self) {
      g_channels[i] = NULL; // don't let a re-created channel alias freed storage
    }
  }
}

void BleChannel_ctor(BleChannel* self, BleChannelRole role, const char* device_name, BleConnParams params) {
  assert(self != NULL);
  assert(device_name != NULL);

  memset(self, 0, sizeof(*self)); // conn=NULL, buffers/params clear, rx_lock unlocked
  self->role = role;
  self->device_name = device_name;
  self->params = params;
  self->tx_payload = BLE_DEFAULT_TX_PAYLOAD;
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
  k_work_init(&self->recover_work, ble_recover_work_handler);

  // BLE requires supervision_timeout > (1 + latency) * interval * 2 in native
  // units (timeout*10ms, interval*1.25ms) that is timeout*4 > (1 + latency)*interval.
  if ((uint32_t)params.supervision_timeout * 4 <= (uint32_t)(1 + params.latency) * params.interval) {
    BLE_CHANNEL_WARN("connection params for '%s' violate the supervision-timeout constraint "
                     "(interval=%u, latency=%u, timeout=%u); the controller may reject them",
                     device_name, params.interval, params.latency, params.supervision_timeout);
  }

  self->super.super.mode = NETWORK_CHANNEL_MODE_POLLED;
  self->super.super.expected_connect_duration = BLE_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.super.type = NETWORK_CHANNEL_TYPE_BLE;
  self->super.super.is_connected = BleChannel_is_connected;
  self->super.super.open_connection = BleChannel_open_connection;
  self->super.super.close_connection = BleChannel_close_connection;
  self->super.super.send_blocking = BleChannel_send_blocking;
  self->super.super.register_receive_callback = BleChannel_register_receive_callback;
  self->super.super.free = BleChannel_free;
  self->super.poll = BleChannel_poll;

  // Register so the process-global BLE callbacks can route to this channel.
  if (g_num_channels >= BLE_MAX_CHANNELS) {
    BLE_CHANNEL_ERR("too many BLE channels (max %d = CONFIG_BT_MAX_CONN); '%s' will not connect", BLE_MAX_CHANNELS,
                    device_name);
    self->state = NETWORK_CHANNEL_STATE_CONNECTION_FAILED;
    return;
  }
  g_channels[g_num_channels++] = self;

  // Bring up the Bluetooth stack once.
  if (!g_bt_ready) {
    int err = bt_enable(NULL);
    if (err) {
      BLE_CHANNEL_ERR("bt_enable failed (%d) - BLE channel will not connect", err);
      self->state = NETWORK_CHANNEL_STATE_CONNECTION_FAILED;
      return;
    }
    g_bt_ready = true;
    BLE_CHANNEL_INFO("Bluetooth stack enabled");
  }

  if (role == BLE_CHANNEL_ROLE_PERIPHERAL) {
    int err = bt_set_name(device_name); // requires CONFIG_BT_DEVICE_NAME_DYNAMIC=y
    if (err) {
      BLE_CHANNEL_WARN("bt_set_name('%s') failed (%d)", device_name, err);
    }
  }

  self->state = NETWORK_CHANNEL_STATE_OPEN;
}
