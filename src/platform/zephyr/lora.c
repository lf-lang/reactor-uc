#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/environment.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/devicetree.h>
#include <zephyr/irq.h>
#include <string.h>
#include <assert.h>

#define LORA_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "LoRaPollChannel: " fmt, ##__VA_ARGS__)
#define LORA_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "LoRaPollChannel: " fmt, ##__VA_ARGS__)
#define LORA_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "LoRaPollChannel: " fmt, ##__VA_ARGS__)
#define LORA_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "LoRaPollChannel: " fmt, ##__VA_ARGS__)

#define LORA_FREQUENCY 868000000
#define LORA_BANDWIDTH BW_125_KHZ
#define LORA_DATARATE SF_7
#define LORA_PREAMBLE_LEN 8
#define LORA_CODING_RATE CR_4_5
#define LORA_TX_POWER 14

typedef struct {
  uint16_t src_node;
  uint16_t dst_node;
  uint32_t payload_len;
  uint8_t payload[LORA_CHANNEL_BUFFERSIZE];
} __attribute__((packed)) LoRaHeaderFrame;

static inline const struct device* get_lora_device(void) {
  return DEVICE_DT_GET(DT_ALIAS(lora0));
}

static void fill_modem_config(struct lora_modem_config* config, bool tx) {
  memset(config, 0, sizeof(*config));
  config->frequency = LORA_FREQUENCY;
  config->bandwidth = LORA_BANDWIDTH;
  config->datarate = LORA_DATARATE;
  config->preamble_len = LORA_PREAMBLE_LEN;
  config->coding_rate = LORA_CODING_RATE;
  config->tx_power = LORA_TX_POWER;
  config->tx = tx;
}

static lf_ret_t configure_lora_modem(const struct device* dev, bool tx) {
  struct lora_modem_config config;
  fill_modem_config(&config, tx);

  if (lora_config(dev, &config) < 0) {
    LORA_CHANNEL_ERR("Failed to configure LoRa modem (%s)", tx ? "TX" : "RX");
    return LF_ERR;
  }

  return LF_OK;
}

static void LoRaPollChannel_rx_cb(const struct device* dev, uint8_t* data, uint16_t size, int16_t rssi, int8_t snr,
                                  void* user_data) {
  (void)dev;
  LoRaPollChannel* self = (LoRaPollChannel*)user_data;
  unsigned int key = irq_lock();

  if (!self->rx_pending && size > 0 && size <= LORA_FRAME_MAX_SIZE) {
    memcpy(self->receive_buffer, data, size);
    self->rx_len = size;
    self->rx_rssi = rssi;
    self->rx_snr = snr;
    self->rx_pending = true;
  }

  irq_unlock(key);

  if (_lf_environment != NULL && _lf_environment->platform != NULL) {
    _lf_environment->platform->notify(_lf_environment->platform);
  }
}

static lf_ret_t start_async_receive(LoRaPollChannel* self) {
  const struct device* dev = get_lora_device();

  if (configure_lora_modem(dev, false) != LF_OK) {
    return LF_ERR;
  }

  if (lora_recv_async(dev, LoRaPollChannel_rx_cb, self) < 0) {
    LORA_CHANNEL_ERR("Failed to start asynchronous LoRa reception");
    return LF_ERR;
  }

  return LF_OK;
}

static void stop_async_receive(void) {
  const struct device* dev = get_lora_device();
  (void)lora_recv_async(dev, NULL, NULL);
}

static bool LoRaPollChannel_is_connected(NetworkChannel* untyped_self) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t LoRaPollChannel_open_connection(NetworkChannel* untyped_self) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  const struct device* dev = get_lora_device();

  if (!device_is_ready(dev)) {
    LORA_CHANNEL_ERR("LoRa hardware device not ready");
    return LF_ERR;
  }

  /* Program TX settings first so the driver stores tx_cfg, then listen. */
  if (configure_lora_modem(dev, true) != LF_OK) {
    return LF_ERR;
  }

  if (start_async_receive(self) != LF_OK) {
    return LF_ERR;
  }

  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
  LORA_CHANNEL_INFO("Connection opened between node %d -> node %d", self->local_node_id, self->destination_node_id);
  return LF_OK;
}

static void LoRaPollChannel_close_connection(NetworkChannel* untyped_self) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  stop_async_receive();
  self->state = NETWORK_CHANNEL_STATE_CLOSED;
}

static void LoRaPollChannel_free(NetworkChannel* untyped_self) {
  stop_async_receive();
  (void)untyped_self;
}

static lf_ret_t LoRaPollChannel_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  const struct device* dev = get_lora_device();

  if (self->state != NETWORK_CHANNEL_STATE_CONNECTED) {
    LORA_CHANNEL_ERR("Cannot send: Channel is not connected");
    return LF_ERR;
  }

  int payload_size = serialize_to_protobuf(message, self->write_buffer, LORA_CHANNEL_BUFFERSIZE);
  if (payload_size <= 0) {
    LORA_CHANNEL_ERR("Failed to serialize FederateMessage");
    return LF_ERR;
  }

  LoRaHeaderFrame frame;
  frame.src_node = self->local_node_id;
  frame.dst_node = self->destination_node_id;
  frame.payload_len = (uint32_t)payload_size;
  memcpy(frame.payload, self->write_buffer, payload_size);

  size_t total_tx_bytes = LORA_FRAME_HEADER_SIZE + (size_t)payload_size;

  stop_async_receive();
  if (configure_lora_modem(dev, true) != LF_OK) {
    (void)start_async_receive(self);
    return LF_ERR;
  }

  int ret = lora_send(dev, (uint8_t*)&frame, total_tx_bytes);
  lf_ret_t rx_ret = start_async_receive(self);

  if (ret < 0) {
    LORA_CHANNEL_ERR("lora_send failed with error: %d", ret);
    return LF_ERR;
  }
  if (rx_ret != LF_OK) {
    return LF_ERR;
  }

  LORA_CHANNEL_INFO("Sent message type %d (%d bytes) to Node %d", message->which_message, total_tx_bytes,
                    self->destination_node_id);
  return LF_OK;
}

static void LoRaPollChannel_register_receive_callback(NetworkChannel* untyped_self,
                                                       void (*receive_callback)(FederatedConnectionBundle* conn,
                                                                                const FederateMessage* msg),
                                                       FederatedConnectionBundle* conn) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

lf_ret_t LoRaPollChannel_poll(NetworkChannel* untyped_self) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  uint8_t snapshot[LORA_FRAME_MAX_SIZE];
  uint16_t bytes_rcvd;
  uint16_t src_node;
  uint16_t dst_node;
  uint32_t payload_len;
  int16_t rssi;
  int8_t snr;
  unsigned int key = irq_lock();

  if (!self->rx_pending) {
    irq_unlock(key);
    return LF_NETWORK_CHANNEL_EMPTY;
  }

  bytes_rcvd = self->rx_len;
  rssi = self->rx_rssi;
  snr = self->rx_snr;
  memcpy(snapshot, self->receive_buffer, bytes_rcvd);
  self->rx_pending = false;
  irq_unlock(key);

  if (bytes_rcvd < LORA_FRAME_HEADER_SIZE) {
    LORA_CHANNEL_WARN("Dropping undersized LoRa frame (%u bytes)", bytes_rcvd);
    return LF_NETWORK_CHANNEL_EMPTY;
  }

  memcpy(&src_node, snapshot, sizeof(src_node));
  memcpy(&dst_node, snapshot + sizeof(src_node), sizeof(dst_node));
  memcpy(&payload_len, snapshot + sizeof(src_node) + sizeof(dst_node), sizeof(payload_len));

  if (payload_len > LORA_CHANNEL_BUFFERSIZE || bytes_rcvd < LORA_FRAME_HEADER_SIZE + payload_len) {
    LORA_CHANNEL_WARN("Dropping malformed LoRa frame (len=%u payload_len=%u)", bytes_rcvd, payload_len);
    return LF_NETWORK_CHANNEL_EMPTY;
  }

  if (dst_node != self->local_node_id && dst_node != 0xFFFF) {
    LORA_CHANNEL_DEBUG("Dropping packet for node %d (my node id: %d)", dst_node, self->local_node_id);
    return LF_NETWORK_CHANNEL_EMPTY;
  }

  LORA_CHANNEL_INFO("Packet received from Node %d [RSSI: %d dBm, SNR: %d dB]", src_node, rssi, snr);

  int bytes_left = deserialize_from_protobuf(&self->output, snapshot + LORA_FRAME_HEADER_SIZE, payload_len);
  if (bytes_left != 0) {
    LORA_CHANNEL_ERR("Deserialization error (bytes_left=%d)", bytes_left);
    return LF_ERR;
  }

  if (self->receive_callback != NULL) {
    self->receive_callback(self->federated_connection, &self->output);
    return LF_NETWORK_CHANNEL_RETRY;
  }

  LORA_CHANNEL_WARN("No receive callback registered, dropping message");
  return LF_ERR;
}

void LoRaPollChannel_ctor(LoRaPollChannel* self, uint16_t local_node_id, uint16_t destination_node_id) {
  assert(self != NULL);

  self->super.super.mode = NETWORK_CHANNEL_MODE_POLLED;
  self->super.super.expected_connect_duration = SEC(0);
  self->super.super.type = NETWORK_CHANNEL_TYPE_LORA;
  self->super.super.is_connected = LoRaPollChannel_is_connected;
  self->super.super.open_connection = LoRaPollChannel_open_connection;
  self->super.super.close_connection = LoRaPollChannel_close_connection;
  self->super.super.send_blocking = LoRaPollChannel_send_blocking;
  self->super.super.register_receive_callback = LoRaPollChannel_register_receive_callback;
  self->super.super.free = LoRaPollChannel_free;
  self->super.poll = LoRaPollChannel_poll;

  self->local_node_id = local_node_id;
  self->destination_node_id = destination_node_id;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
  self->rx_pending = false;
  self->rx_len = 0;
  self->rx_rssi = 0;
  self->rx_snr = 0;

  memset(self->receive_buffer, 0, LORA_FRAME_MAX_SIZE);
  memset(self->write_buffer, 0, LORA_CHANNEL_BUFFERSIZE);
}
