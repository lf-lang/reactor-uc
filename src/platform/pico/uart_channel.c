#include "reactor-uc/platform/pico/uart_channel.h"
#include "reactor-uc/platform/pico/pico.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/environment.h"

#define UART_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "UartPolledChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "UartPolledChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "UartPolledChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "UartPolledChannel: " fmt, ##__VA_ARGS__)

#define UART_OPEN_MESSAGE_REQUEST {0xC0, 0x18, 0x11, 0xC0, 0xDD}
#define UART_OPEN_MESSAGE_RESPONSE {0xC0, 0xFF, 0x31, 0xC0, 0xDD}
#define UART_MESSAGE_PREFIX {0xAA, 0xAA, 0xAA, 0xAA, 0xAA}
#define UART_CLOSE_MESSAGE {0x2, 0xF, 0x6, 0xC, 0x2};
#define MINIMUM_MESSAGE_SIZE 9
#define UART_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(2500)

static UartPolledChannel *uart_channel_0 = NULL;
static UartPolledChannel *uart_channel_1 = NULL;

static lf_ret_t UartPolledChannel_open_connection(NetworkChannel *untyped_self) {
  UartPolledChannel *self = (UartPolledChannel *)untyped_self;
  char connect_message[] = UART_OPEN_MESSAGE_REQUEST;
  uart_write_blocking(self->uart_device, (const uint8_t *)connect_message, sizeof(connect_message));
  UART_CHANNEL_DEBUG("Open connection");
  return LF_OK;
}

static void UartPolledChannel_close_connection(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Close connection");
  (void)untyped_self;
}

static void UartPolledChannel_free(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Free");
  (void)untyped_self;
}

static bool UartPolledChannel_is_connected(NetworkChannel *untyped_self) {
  UartPolledChannel *self = (UartPolledChannel *)untyped_self;
  UART_CHANNEL_DEBUG("Open connection - Sending Ping");

  if (!self->received_response) {
    char connect_message[] = UART_OPEN_MESSAGE_REQUEST;
    uart_write_blocking(self->uart_device, (const uint8_t *)connect_message, sizeof(connect_message));
  }

  return self->state == NETWORK_CHANNEL_STATE_CONNECTED && self->received_response && self->send_response;
}

static lf_ret_t UartPolledChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  UartPolledChannel *self = (UartPolledChannel *)untyped_self;
  char uart_message_prefix[] = UART_MESSAGE_PREFIX;
  memcpy(self->send_buffer, uart_message_prefix, sizeof(uart_message_prefix));
  int message_size =
      serialize_to_protobuf(message, self->send_buffer + sizeof(uart_message_prefix), UART_CHANNEL_BUFFERSIZE);
  // UART_CHANNEL_DEBUG("Sending Message of Size: %i", message_size);
  uart_write_blocking(self->uart_device, (const uint8_t *)self->send_buffer,
                      message_size + sizeof(uart_message_prefix));
  return LF_OK;
}

static void UartPolledChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                        void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                                 const FederateMessage *message),
                                                        FederatedConnectionBundle *bundle) {
  UART_CHANNEL_DEBUG("Register receive callback");
  UartPolledChannel *self = (UartPolledChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->bundle = bundle;
}

void _UartPolledChannel_interrupt_handler(UartPolledChannel *self) {
  while (uart_is_readable(self->uart_device)) {
    uint8_t received_byte = uart_getc(self->uart_device);
    self->receive_buffer[self->receive_buffer_index] = received_byte;
    self->receive_buffer_index++;

    if (received_byte == 0xDD && self->receive_buffer_index >= 5) {
      char request_message[] = UART_OPEN_MESSAGE_REQUEST;
      char response_message[] = UART_OPEN_MESSAGE_RESPONSE;
      if (memcmp(request_message, &self->receive_buffer[self->receive_buffer_index - sizeof(request_message)],
                 sizeof(request_message)) == 0) {
        self->receive_buffer_index -= sizeof(request_message);
        printf("Found Request Signature - Responding\n");
        self->send_response = true;
        uart_write_blocking(self->uart_device, (const uint8_t *)response_message, sizeof(response_message));
      } else if (memcmp(response_message, &self->receive_buffer[self->receive_buffer_index - sizeof(response_message)],
                        sizeof(response_message)) == 0) {
        self->receive_buffer_index -= sizeof(response_message);
        printf("Found Response Signature - Updating State\n");
        self->state = NETWORK_CHANNEL_STATE_CONNECTED;
        self->received_response = true;
        _lf_environment->platform->new_async_event(_lf_environment->platform);
      }
    }
  }
  if (self->receive_buffer_index > MINIMUM_MESSAGE_SIZE) {
    _lf_environment->platform->new_async_event(_lf_environment->platform);
  }
}

static void _UartPolledChannel_pico_interrupt_handler_0(void) {
  if (uart_channel_0 != NULL) {
    _UartPolledChannel_interrupt_handler(uart_channel_0);
  }
}

static void _UartPolledChannel_pico_interrupt_handler_1(void) {
  if (uart_channel_1 != NULL) {
    _UartPolledChannel_interrupt_handler(uart_channel_1);
  }
}

void UartPolledChannel_poll(PolledNetworkChannel *untyped_self) {
  UartPolledChannel *self = (UartPolledChannel *)untyped_self;

  while (self->receive_buffer_index > MINIMUM_MESSAGE_SIZE) {
    char uart_message_prefix[] = UART_MESSAGE_PREFIX;
    int message_start_index = -1;
    for (int i = 0; i < (int)(self->receive_buffer_index - sizeof(uart_message_prefix)); i++) {
      if (memcmp(uart_message_prefix, &self->receive_buffer[i], sizeof(uart_message_prefix)) == 0) {
        message_start_index = i;
        break;
      }
    }

    if (message_start_index == -1) {
      self->receive_buffer_index = 0;
    }

    // UART_CHANNEL_DEBUG("Message prefix found at index %i", message_start_index);
    message_start_index += sizeof(uart_message_prefix);

    int bytes_left = deserialize_from_protobuf(&self->output, self->receive_buffer + message_start_index,
                                               self->receive_buffer_index - message_start_index);
    // UART_CHANNEL_DEBUG("Bytes Left after attempted to deserialize %d index: %d", bytes_left,
    // self->receive_buffer_index);

    if (bytes_left >= 0) {
      //_lf_environment->enter_critical_section(_lf_environment);
      int receive_buffer_index = self->receive_buffer_index;
      self->receive_buffer_index = bytes_left;
      memcpy(self->receive_buffer, self->receive_buffer + (receive_buffer_index - bytes_left), bytes_left);
      //_lf_environment->leave_critical_section(_lf_environment);

      // TODO: we potentially can move this memcpy out of the critical section

      // UART_CHANNEL_DEBUG("callback: %p", self->receive_callback);
      if (self->receive_callback != NULL) {
        // UART_CHANNEL_DEBUG("calling user callback!");
        self->receive_callback(self->bundle, &self->output);
      }
    } else {
      break;
    }
  }
}

static unsigned int from_uc_data_bits(UartDataBits data_bits) {
  switch (data_bits) {
  case UC_UART_DATA_BITS_5:
    return 5;
  case UC_UART_DATA_BITS_6:
    return 6;
  case UC_UART_DATA_BITS_7:
    return 7;
  case UC_UART_DATA_BITS_8:
    return 8;
  };

  return 8;
}

static uart_parity_t from_uc_parity_bits(UartParityBits parity_bits) {
  switch (parity_bits) {
  case UC_UART_PARITY_NONE:
    return UART_PARITY_NONE;
  case UC_UART_PARITY_EVEN:
    return UART_PARITY_EVEN;
  case UC_UART_PARITY_ODD:
    return UART_PARITY_ODD;
  case UC_UART_PARITY_MARK:
    throw("Not supported by pico SDK");
  case UC_UART_PARITY_SPACE:
    throw("Not supported by pico SDK");
  }

  return UART_PARITY_EVEN;
}

static unsigned int from_uc_stop_bits(UartStopBits stop_bits) {
  switch (stop_bits) {
  case UC_UART_STOP_BITS_1:
    return 1;
  case UC_UART_STOP_BITS_2:
    return 2;
  }

  return 2;
}

void UartPolledChannel_ctor(UartPolledChannel *self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity_bits, UartStopBits stop_bits) {

  assert(self != NULL);

  // Concrete fields
  self->receive_buffer_index = 0;
  self->receive_callback = NULL;
  self->bundle = NULL;
  self->send_response = false;
  self->received_response = false;

  self->state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
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

  int rx_pin;
  int tx_pin;
  if (uart_device == 0) {
    self->uart_device = uart0;
    uart_channel_0 = self;
    rx_pin = 1;
    tx_pin = 0;
  } else if (uart_device == 1) {
    self->uart_device = uart1;
    uart_channel_1 = self;
    rx_pin = 9;
    tx_pin = 8;
  } else {
    throw("The Raspberry Pi pico only supports uart devices 0 and 1.");
  }

  uart_init(self->uart_device, 2400);
  gpio_set_function(tx_pin, UART_FUNCSEL_NUM(self->uart_device, tx_pin));
  gpio_set_function(rx_pin, UART_FUNCSEL_NUM(self->uart_device, rx_pin));
  int actual = uart_set_baudrate(self->uart_device, baud);

  if (actual != (int)baud) {
    UART_CHANNEL_WARN("Other baudrate then specified got configured requested: %d actual %d", baud, actual);
  }

  uart_set_hw_flow(self->uart_device, false, false);

  uart_set_format(self->uart_device, from_uc_data_bits(data_bits), from_uc_stop_bits(stop_bits),
                  from_uc_parity_bits(parity_bits));

  uart_set_fifo_enabled(self->uart_device, false);

  irq_set_exclusive_handler(UART0_IRQ, _UartPolledChannel_pico_interrupt_handler_0);
  irq_set_exclusive_handler(UART1_IRQ, _UartPolledChannel_pico_interrupt_handler_1);
  irq_set_enabled(UART0_IRQ, true);
  irq_set_enabled(UART1_IRQ, true);

  uart_set_irq_enables(self->uart_device, true, false);
}