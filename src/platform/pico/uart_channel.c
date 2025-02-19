#include "reactor-uc/platform/pico/uart_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"

#define UART_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "UartPollChannel: " fmt, ##__VA_ARGS__)

static lf_ret_t UartPollChannel_open_connection(NetworkChannel *untyped_self) {

extern Environment *_lf_environment;

UartPollChannel* uart_channel_0 = NULL;
UartPollChannel* uart_channel_1 = NULL;

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

static bool UartPollChannel_was_ever_connected(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("UARTChannel is connected!");
  (void)untyped_self;

  return true;
}

static bool UartPollChannel_is_connected(NetworkChannel *untyped_self) {
  UartPollChannel *self = (UartPollChannel *)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t UartPollChannel_send_blocking(NetworkChannel *untyped_self, const char *message,
                                                size_t message_size) {
  UartPollChannel *self = (UartPollChannel *)untyped_self;

  if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
    UART_CHANNEL_DEBUG("Sending Message of Size: %i", message_size);
    uart_write_blocking(self->uart_device, (const uint8_t*)message, message_size);
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

void _UartPollChannel_interrupt_handler(UartPollChannel* self) {
  while (uart_is_readable(self->uart_device)) {
    uint8_t received_byte = uart_getc(self->uart_device);
    self->receive_buffer_index++;
    self->receive_buffer[self->receive_buffer_index] = received_byte;

    if (self->receive_buffer_index > sizeof(MessageFraming)) {
      int available = sem_available(&((PlatformPico*)_lf_environment->platform)->sem);

      if (available <= 0) {
        // this should wake up the interruptable sleep
        sem_release(&((PlatformPico*)_lf_environment->platform)->sem);
      }
    }
  }
}

void _UartPollChannel_pico_interrupt_handler(void) {
   if (uart_channel_0 != NULL) {
     _UartPollChannel_interrupt_handler(uart_channel_0);
   }
   if (uart_channel_1 != NULL) {
     _UartPollChannel_interrupt_handler(uart_channel_1);
   }
}

void UartPollChannel_poll(PollNetworkChannel *untyped_self) {
  UartPollChannel *self = (UartPollChannel *)untyped_self;

  while (self->receive_buffer_index > sizeof(MessageFraming)) {
    MessageFraming *frame = (MessageFraming *)self->receive_buffer;
    UART_CHANNEL_DEBUG("Message Size %d", frame->message_size);
    if (self->receive_buffer_index >= frame->message_size) {
      if (self->receive_callback != NULL) {
        UART_CHANNEL_DEBUG("calling user callback!");
        self->receive_callback(self->encryption_layer, (const char *)&self->receive_buffer, frame->message_size);
      }

      self->receive_buffer_index = 0;
    }
  }
}

unsigned int from_uc_data_bits(UartDataBits data_bits) {
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

uart_parity_t from_uc_parity_bits(UartParityBits parity_bits) {
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

unsigned int from_uc_stop_bits(UartStopBits stop_bits) {
  switch (stop_bits) {
  case UC_UART_STOP_BITS_1:
    return 1;
  case UC_UART_STOP_BITS_2:
    return 2;
  }

  return 2;
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
  self->super.super.is_connected = UartPollChannel_is_connected;
  self->super.super.open_connection = UartPollChannel_open_connection;
  self->super.super.close_connection = UartPollChannel_close_connection;
  self->super.super.send_blocking = UartPollChannel_send_blocking;
  self->super.super.register_receive_callback = UartPollChannel_register_receive_callback;
  self->super.super.free = UartPollChannel_free;
  self->super.super.was_ever_connected = UartPollChannel_was_ever_connected;
  self->super.poll = UartPollChannel_poll;

  if (uart_device == 0) {
    self->uart_device = uart0;
    uart_channel_0 = self;
  } else if (uart_device == 1) {
    self->uart_device = uart1;
    uart_channel_1 = self;
  } else {
    throw("The Raspberry Pi pico only supports uart devices 0 and 1.");
  }

#define UART_TX_PIN 0
#define UART_RX_PIN 1
  // Set up our UART with a basic baud rate.
  uart_init(self->uart_device, 2400);

  // Set the TX and RX pins by using the function select on the GPIO
  // Set datasheet for more information on function select
  gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(self->uart_device, UART_TX_PIN));
  gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(self->uart_device, UART_RX_PIN));

  // Actually, we want a different speed
  // The call will return the actual baud rate selected, which will be as close as
  // possible to that requested
  int actual = uart_set_baudrate(self->uart_device, baud);

  if (actual != (int)baud) {
    UART_CHANNEL_WARN("Other baudrate then specified got configured requested: %d actual %d", baud, actual);
  }

  // Set UART flow control CTS/RTS, we don't want these, so turn them off
  uart_set_hw_flow(self->uart_device, false, false);

  // Set our data format
  uart_set_format(self->uart_device,
                  from_uc_data_bits(data_bits),
                  from_uc_stop_bits(stop_bits),
                  from_uc_parity_bits(parity_bits));

  // Turn off FIFO's - we want to do this character by character
  uart_set_fifo_enabled(self->uart_device, false);

  // Set up a RX interrupt
  // We need to set up the handler first
  // Select correct interrupt for the UART we are using
  int UART_IRQ = (self->uart_device == uart0) ? UART0_IRQ : UART1_IRQ;

  // And set up and enable the interrupt handlers
  irq_set_exclusive_handler(UART_IRQ, _UartPollChannel_pico_interrupt_handler);
  irq_set_enabled(UART_IRQ, true);

  // Now enable the UART to send interrupts - RX only
  uart_set_irq_enables(self->uart_device, true, false);
}

