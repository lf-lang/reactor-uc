#include "reactor-uc/platform/riot/uart_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"

#define UART_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)

static lf_ret_t UartPollChannel_open_connection(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Open connection");
  (void)untyped_self;
  return LF_OK;
}

static void UartPollChannel_close_connection(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Close connection");
  (void)untyped_self;
}

static void UartPollChannel_free(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Free");
  (void)untyped_self;
}

static bool UartPolledChannel_is_connected(NetworkChannel *untyped_self) {
  UartPolledChannel *self = (UartPolledChannel *)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t UartPollChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  UartPollChannel *self = (UartPollChannel *)untyped_self;

  if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
    UART_CHANNEL_DEBUG("Sending Message of Size: %i", message_size);
    uart_write(self->uart_dev, (const uint8_t *)message, message_size);
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

static void UartPollChannel_register_receive_callback(NetworkChannel *untyped_self, EncryptionLayer *encryption_layer,
                                                      void (*receive_callback)(EncryptionLayer *encryption_layer,
                                                                               const char *message, ssize_t size)) {
  UART_CHANNEL_INFO("Register receive callback");
  UartPollChannel *self = (UartPollChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->encryption_layer = encryption_layer;
}

void _UartPollChannel_interrupt_callback(void *arg, uint8_t received_byte) {
  UartPollChannel *self = (UartPollChannel *)arg;
  const uint32_t minimum_message_size = 12;

  int receive_buffer_index = self->receive_buffer_index;
  self->receive_buffer_index++;

  self->receive_buffer[receive_buffer_index] = received_byte;

  if (self->receive_buffer_index >= minimum_message_size) {
    if (self->super.super.mode == NETWORK_CHANNEL_MODE_ASYNC) {
      cond_signal(&((UartAsyncChannel *)self)->receive_cv);
    }
  }
}

void UartPollChannel_poll(NetworkChannel *untyped_self) {
  UartPollChannel *self = (UartPollChannel *)untyped_self;
  const uint32_t minimum_message_size = 12;

  while (self->receive_buffer_index > minimum_message_size) {
    int bytes_left = deserialize_from_protobuf(&self->output, self->receive_buffer, self->receive_buffer_index);
    UART_CHANNEL_DEBUG("Bytes Left after attempted to deserialize %d", bytes_left);

    if (bytes_left >= 0) {
      _lf_environment->enter_critical_section(_lf_environment);
      int receive_buffer_index = self->receive_buffer_index;
      self->receive_buffer_index = bytes_left;
      memcpy(self->receive_buffer, self->receive_buffer + (receive_buffer_index - bytes_left), bytes_left);
      _lf_environment->leave_critical_section(_lf_environment);

      // TODO: we potentially can move this memcpy out of the critical section
      if (self->receive_callback != NULL) {
        UART_CHANNEL_DEBUG("calling user callback!");
        self->receive_callback(self->encryption_layer, (const char *)&self->receive_buffer, frame->message_size);
      }

      self->receive_buffer_index = 0;
    }
  }
}

void *_UartAsyncChannel_decode_loop(void *arg) {
  UartAsyncChannel *self = (UartAsyncChannel *)arg;
  mutex_lock(&self->receive_lock);

  UART_CHANNEL_DEBUG("Entering decoding loop");
  while (true) {
    cond_wait(&self->receive_cv, &self->receive_lock);

    UartPollChannel_poll((NetworkChannel *)&self->super);
  }

  return NULL;
}

uart_data_bits_t from_uc_data_bits(UartDataBits data_bits) {
  switch (data_bits) {
  case UC_UART_DATA_BITS_5:
    return UART_DATA_BITS_5;
  case UC_UART_DATA_BITS_6:
    return UART_DATA_BITS_6;
  case UC_UART_DATA_BITS_7:
    return UART_DATA_BITS_7;
  case UC_UART_DATA_BITS_8:
    return UART_DATA_BITS_8;
  };

  return UART_DATA_BITS_8;
}

uart_parity_t from_uc_parity_bits(UartParityBits parity_bits) {
  switch (parity_bits) {
  case UC_UART_PARITY_NONE:
    return UART_PARITY_NONE;
  case UC_UART_PARITY_EVEN:
    return UART_PARITY_EVEN;
  case UC_UART_PARITY_ODD:
    return UART_PARITY_ODD;
  case UC_UART_PARITY_MARK:
    return UART_PARITY_MARK;
  case UC_UART_PARITY_SPACE:
    return UART_PARITY_SPACE;
  }

  return UART_PARITY_EVEN;
}

uart_stop_bits_t from_uc_stop_bits(UartStopBits stop_bits) {
  switch (stop_bits) {
  case UC_UART_STOP_BITS_1:
    return UART_STOP_BITS_1;
  case UC_UART_STOP_BITS_2:
    return UART_STOP_BITS_2;
  }

  return UART_STOP_BITS_2;
}

void UartPollChannel_ctor(UartPollChannel *self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                          UartParityBits parity_bits, UartStopBits stop_bits) {

  assert(self != NULL);

  // Concrete fields
  self->receive_buffer_index = 0;
  self->receive_callback = NULL;
  self->encryption_layer = NULL;
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;

  self->super.super.mode = NETWORK_CHANNEL_MODE_POLLED;
  self->super.super.expected_connect_duration = UART_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.super.type = NETWORK_CHANNEL_TYPE_UART;
  self->super.super.is_connected = UartPolledChannel_is_connected;
  self->super.super.open_connection = UartPolledChannel_open_connection;
  self->super.super.close_connection = UartPolledChannel_close_connection;
  self->super.super.send_blocking = UartPolledChannel_send_blocking;
  self->super.super.register_receive_callback = UartPolledChannel_register_receive_callback;
  self->super.super.free = UartPolledChannel_free;
  self->super.poll = UartPolledChannel_poll;
  self->uart_dev = UART_DEV(uart_device);

  int result = uart_init(self->uart_dev, baud, _UartPollChannel_interrupt_callback, self);

  if (result == -ENODEV) {
    UART_CHANNEL_ERR("Invalid UART device!");
    throw("The user specified an invalid UART device!");
  } else if (result == ENOTSUP) {
    UART_CHANNEL_ERR("Given configuration to uart is not supported!");
    throw("The given combination of parameters for creating a uart device is unsupported!");
  } else if (result < 0) {
    UART_CHANNEL_ERR("UART Init error occurred");
    throw("Unknown UART RIOT Error!");
  }

  result = uart_mode(self->uart_dev, from_uc_data_bits(data_bits), from_uc_parity_bits(parity_bits),
                     from_uc_stop_bits(stop_bits));

  if (result != UART_OK) {
    UART_CHANNEL_ERR("Problem to configure UART device!");
    throw("RIOT was unable to configure the UART device!");
  }
}

void UartAsyncChannel_ctor(UartAsyncChannel *self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                           UartParityBits parity, UartStopBits stop_bits) {

  UartPollChannel_ctor(&self->super, uart_device, baud, data_bits, parity, stop_bits);

  cond_init(&self->receive_cv);
  mutex_init(&self->receive_lock);

  // Create decoding thread
  self->decode_thread_pid =
      thread_create(self->decode_thread_stack, sizeof(self->decode_thread_stack), THREAD_PRIORITY_MAIN - 1, 0,
                    _UartAsyncChannel_decode_loop, self, "uart_channel_decode_loop");

  self->super.super.super.mode = NETWORK_CHANNEL_MODE_ASYNC;
}
