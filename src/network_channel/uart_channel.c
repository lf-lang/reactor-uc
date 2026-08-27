#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"

#ifdef NETWORK_CHANNEL_UART

#define UART_CORE_ERR(fmt, ...) LF_ERR(NET, "UartChannel: " fmt, ##__VA_ARGS__)
#define UART_CORE_WARN(fmt, ...) LF_WARN(NET, "UartChannel: " fmt, ##__VA_ARGS__)
#define UART_CORE_DEBUG(fmt, ...) LF_DEBUG(NET, "UartChannel: " fmt, ##__VA_ARGS__)

static inline unsigned int ring_next(unsigned int i) { return (i + 1U) % UART_CORE_RX_RING_SIZE; }

bool UartChannelCore_rx_push(UartChannelCore* self, const unsigned char* data, size_t len) {
  bool frame_boundary = false;

  for (size_t i = 0; i < len; i++) {
    unsigned int next = ring_next(self->head);
    if (next == self->tail) {
      // Drop the bytes that don't fit in the ring buffer.
      self->stat_ring_overflow++;
      break;
    }
    self->ring[self->head] = data[i];
    self->head = next;
    if (data[i] == LF_FRAME_DELIMITER) {
      frame_boundary = true;
    }
  }

  return frame_boundary;
}

uint32_t lf_uart_tx_timeout_ms(uint32_t baud, size_t len) {
  if (baud == 0) {
    return 1000;
  }
  const uint64_t wire_ms = ((uint64_t)len * 12ULL * 1000ULL) / baud;
  return (uint32_t)(wire_ms * 4ULL) + 10U;
}

void UartChannelCore_notify(void) {
  // Notify the environment of a pending message.
  if (_lf_environment != NULL && _lf_environment->platform != NULL) {
    _lf_environment->platform->notify(_lf_environment->platform);
  }
}

static bool ring_pop(UartChannelCore* self, unsigned char* out) {
  if (self->tail == self->head) {
    return false;
  }
  *out = self->ring[self->tail];
  self->tail = ring_next(self->tail);
  return true;
}

static bool UartChannelCore_is_connected(NetworkChannel* untyped_self) {
  UartChannelCore* self = (UartChannelCore*)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t UartChannelCore_open_connection(NetworkChannel* untyped_self) {
  UartChannelCore* self = (UartChannelCore*)untyped_self;
  if (self->state == NETWORK_CHANNEL_STATE_UNINITIALIZED) {
    // The platform constructor must call UartChannelCore_ctor() first.
    return LF_ERR;
  }
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
  UART_CORE_DEBUG("Connection open");
  return LF_OK;
}

static void UartChannelCore_close_connection(NetworkChannel* untyped_self) {
  UartChannelCore* self = (UartChannelCore*)untyped_self;
  self->state = NETWORK_CHANNEL_STATE_CLOSED;
  UART_CORE_DEBUG("Connection closed");
}

static void UartChannelCore_free(NetworkChannel* untyped_self) {
  UartChannelCore* self = (UartChannelCore*)untyped_self;
  if (self->teardown != NULL) {
    self->teardown(self);
  }
  UART_CORE_DEBUG("Freed");
}

static void UartChannelCore_register_receive_callback(NetworkChannel* untyped_self,
                                                      void (*receive_callback)(FederatedConnectionBundle* conn,
                                                                               const FederateMessage* message),
                                                      FederatedConnectionBundle* bundle) {
  UartChannelCore* self = (UartChannelCore*)untyped_self;
  self->receive_callback = receive_callback;
  self->bundle = bundle;
  UART_CORE_DEBUG("Registered receive callback for bundle %p", (void*)bundle);
}

static lf_ret_t UartChannelCore_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  UartChannelCore* self = (UartChannelCore*)untyped_self;
  if (self->write == NULL) {
    return LF_ERR;
  }

  const int payload_len = serialize_to_protobuf(message, self->tx_payload, sizeof(self->tx_payload));
  if (payload_len < 0) {
    UART_CORE_ERR("Failed to serialize message");
    return LF_ERR;
  }

  const size_t frame_len =
      lf_frame_encode(self->tx_payload, (size_t)payload_len, self->send_buffer, sizeof(self->send_buffer));
  if (frame_len == 0) {
    UART_CORE_ERR("Failed to frame message of %d bytes", payload_len);
    return LF_ERR;
  }

  const lf_ret_t ret = self->write(self, self->send_buffer, frame_len);
  if (ret != LF_OK) {
    UART_CORE_ERR("Transmit failed for frame of %zu bytes (%d payload)", frame_len, payload_len);
    return ret;
  }
  UART_CORE_DEBUG("Sent frame of %zu bytes (%d payload)", frame_len, payload_len);
  return LF_OK;
}

static lf_ret_t UartChannelCore_poll(NetworkChannel* untyped_self) {
  UartChannelCore* self = (UartChannelCore*)untyped_self;
  bool processed = false;
  unsigned char byte;

  while (ring_pop(self, &byte)) {
    size_t payload_len = 0;
    const LfFrameStatus st =
        lf_frame_receiver_push(&self->rx, byte, self->rx_payload, sizeof(self->rx_payload), &payload_len);

    if (st == LF_FRAME_OK) {
      const int rem = deserialize_from_protobuf(&self->output, self->rx_payload, payload_len);
      if (rem < 0) {
        // CRC was valid, but the payload was not a valid protobuf.
        // This is a protocol error, but not a link error.
        UART_CORE_WARN("Protobuf decode failed on a CRC-valid frame of %zu bytes", payload_len);
      } else if (self->receive_callback != NULL) {
        self->receive_callback(self->bundle, &self->output);
        processed = true;
      }
    } else if (st < 0) {
      UART_CORE_WARN("Frame error %d (ok=%u crc=%u decode=%u overflow=%u ring=%u)", (int)st, self->rx.stat_frames_ok,
                     self->rx.stat_crc_error, self->rx.stat_decode_error, self->rx.stat_overflow,
                     self->stat_ring_overflow);
    }
  }

  // If we processed at least one frame, return LF_NETWORK_CHANNEL_RETRY
  // to indicate that the caller should poll again.
  return processed ? LF_NETWORK_CHANNEL_RETRY : LF_NETWORK_CHANNEL_EMPTY;
}

void UartChannelCore_ctor(UartChannelCore* self,
                          lf_ret_t (*write)(UartChannelCore* super, const unsigned char* data, size_t len),
                          void (*teardown)(UartChannelCore* super)) {
  self->write = write;
  self->teardown = teardown;
  self->head = 0;
  self->tail = 0;
  self->stat_ring_overflow = 0;
  self->bundle = NULL;
  self->receive_callback = NULL;
  lf_frame_receiver_init(&self->rx);

  self->super.super.mode = NETWORK_CHANNEL_MODE_POLLED;
  self->super.super.expected_connect_duration = UART_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.super.type = NETWORK_CHANNEL_TYPE_UART;
  self->super.super.is_connected = UartChannelCore_is_connected;
  self->super.super.open_connection = UartChannelCore_open_connection;
  self->super.super.close_connection = UartChannelCore_close_connection;
  self->super.super.send_blocking = UartChannelCore_send_blocking;
  self->super.super.register_receive_callback = UartChannelCore_register_receive_callback;
  self->super.super.free = UartChannelCore_free;
  self->super.poll = UartChannelCore_poll;

  self->state = NETWORK_CHANNEL_STATE_OPEN;
}

#endif // NETWORK_CHANNEL_UART
