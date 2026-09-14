#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/devicetree.h>
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
#define LORA_TX_NODE_ID 1

typedef struct {
  uint16_t src_node;
  uint16_t dst_node;
  uint32_t payload_len;
  uint8_t payload[LORA_CHANNEL_BUFFERSIZE];
} __attribute__((packed)) LoRaHeaderFrame;

static inline const struct device* get_lora_device(void) {
  return DEVICE_DT_GET(DT_ALIAS(lora0));
}

static bool LoRaPollChannel_is_connected(NetworkChannel* untyped_self) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t configure_lora_modem(const struct device* dev, bool tx) {
  struct lora_modem_config config = {
      .frequency = LORA_FREQUENCY,
      .bandwidth = LORA_BANDWIDTH,
      .datarate = LORA_DATARATE,
      .preamble_len = LORA_PREAMBLE_LEN,
      .coding_rate = LORA_CODING_RATE,
      .tx_power = LORA_TX_POWER,
      .tx = tx,
  };

  if (lora_config(dev, &config) < 0) {
    LORA_CHANNEL_ERR("Failed to configure LoRa modem");
    return LF_ERR;
  }

  return LF_OK;
}

static lf_ret_t LoRaPollChannel_open_connection(NetworkChannel* untyped_self) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  const struct device* dev = get_lora_device();

  if (!device_is_ready(dev)) {
    LORA_CHANNEL_ERR("LoRa hardware device not ready");
    return LF_ERR;
  }

  if (configure_lora_modem(dev, self->local_node_id == LORA_TX_NODE_ID) != LF_OK) {
    return LF_ERR;
  }

  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
  LORA_CHANNEL_INFO("Connection opened between node %d -> node %d", self->local_node_id, self->destination_node_id);
  return LF_OK;
}

static void LoRaPollChannel_close_connection(NetworkChannel* untyped_self) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  self->state = NETWORK_CHANNEL_STATE_CLOSED;
}

static void LoRaPollChannel_free(NetworkChannel* untyped_self) {
  (void)untyped_self;
}

static lf_ret_t LoRaPollChannel_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  LoRaPollChannel* self = (LoRaPollChannel*)untyped_self;
  const struct device* dev = get_lora_device();

  if (self->state != NETWORK_CHANNEL_STATE_CONNECTED) {
    LORA_CHANNEL_ERR("Cannot send: Channel is not connected");
    return LF_ERR;
  }

  // 1. Serialize Protobuf FederateMessage into byte payload
  int payload_size = serialize_to_protobuf(message, self->write_buffer, LORA_CHANNEL_BUFFERSIZE);
  if (payload_size <= 0) {
    LORA_CHANNEL_ERR("Failed to serialize FederateMessage");
    return LF_ERR;
  }

  // 2. Wrap payload with physical routing header
  LoRaHeaderFrame frame;
  frame.src_node = self->local_node_id;
  frame.dst_node = self->destination_node_id;
  frame.payload_len = (uint32_t)payload_size;
  memcpy(frame.payload, self->write_buffer, payload_size);

  size_t total_tx_bytes = sizeof(frame.src_node) + sizeof(frame.dst_node) + sizeof(frame.payload_len) + payload_size;

  // 3. Transmit packet over LoRa physical layer
  int ret = lora_send(dev, (uint8_t*)&frame, total_tx_bytes);
  if (ret < 0) {
    LORA_CHANNEL_ERR("lora_send failed with error: %d", ret);
    return LF_ERR;
  }

  LORA_CHANNEL_INFO("Sent message type %d (%d bytes) to Node %d", message->which_message, total_tx_bytes,
                    self->destination_node_id);
  LORA_CHANNEL_DEBUG("Sent message type %d (%d bytes) to Node %d", message->which_message, total_tx_bytes, self->destination_node_id);
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
  const struct device* dev = get_lora_device();

  LoRaHeaderFrame frame;
  int16_t rssi;
  int8_t snr;

  // Keep the receiver open long enough to catch a packet on the air.
  int bytes_rcvd = lora_recv(dev, (uint8_t*)&frame, sizeof(frame), K_SECONDS(5), &rssi, &snr);
  if (bytes_rcvd < 0) {
    return LF_NETWORK_CHANNEL_EMPTY; // No packet received
  }

  // Address filtering: ignore messages not destined for this node
  if (frame.dst_node != self->local_node_id && frame.dst_node != 0xFFFF) {
    LORA_CHANNEL_DEBUG("Dropping packet for node %d (my node id: %d)", frame.dst_node, self->local_node_id);
    return LF_NETWORK_CHANNEL_EMPTY;
  }

  LORA_CHANNEL_INFO("Packet received from Node %d [RSSI: %d dBm, SNR: %d dB]", frame.src_node, rssi, snr);

  // Deserialize payload back into FederateMessage struct[cite: 2]
  int bytes_left = deserialize_from_protobuf(&self->output, frame.payload, frame.payload_len);
  if (bytes_left != 0) {
    LORA_CHANNEL_ERR("Deserialization error (bytes_left=%d)", bytes_left);
    return LF_ERR;
  }

  // Trigger federated runtime callback[cite: 2]
  if (self->receive_callback != NULL) {
    self->receive_callback(self->federated_connection, &self->output);
    return LF_NETWORK_CHANNEL_RETRY;
  } else {
    LORA_CHANNEL_WARN("No receive callback registered, dropping message");
    return LF_ERR;
  }
}

void LoRaPollChannel_ctor(LoRaPollChannel* self, uint16_t local_node_id, uint16_t destination_node_id) {
  assert(self != NULL);

  // Populate base NetworkChannel function vtable[cite: 2]
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

  memset(self->receive_buffer, 0, LORA_CHANNEL_BUFFERSIZE);
  memset(self->write_buffer, 0, LORA_CHANNEL_BUFFERSIZE);
}