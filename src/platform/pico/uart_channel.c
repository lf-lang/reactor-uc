#include "reactor-uc/platform/pico/uart_channel.h"
#include "reactor-uc/logging.h"

#define UART_PICO_ERR(fmt, ...) LF_ERR(NET, "UartChannel(pico): " fmt, ##__VA_ARGS__)
#define UART_PICO_WARN(fmt, ...) LF_WARN(NET, "UartChannel(pico): " fmt, ##__VA_ARGS__)
#define UART_PICO_INFO(fmt, ...) LF_INFO(NET, "UartChannel(pico): " fmt, ##__VA_ARGS__)

static UartPolledChannel* uart_channel_0 = NULL;
static UartPolledChannel* uart_channel_1 = NULL;

static unsigned int from_uc_data_bits(UartDataBits data_bits) {
  switch (data_bits) {
  case UC_UART_DATA_BITS_5:
    return 5;
  case UC_UART_DATA_BITS_6:
    return 6;
  case UC_UART_DATA_BITS_7:
    return 7;
  case UC_UART_DATA_BITS_8:
  default:
    return 8;
  }
}

static uart_parity_t from_uc_parity_bits(UartParityBits parity_bits) {
  switch (parity_bits) {
  case UC_UART_PARITY_EVEN:
    return UART_PARITY_EVEN;
  case UC_UART_PARITY_ODD:
    return UART_PARITY_ODD;
  case UC_UART_PARITY_MARK:
  case UC_UART_PARITY_SPACE:
    throw("Mark/space parity is not supported by the pico SDK");
    return UART_PARITY_NONE;
  case UC_UART_PARITY_NONE:
  default:
    return UART_PARITY_NONE;
  }
}

static unsigned int from_uc_stop_bits(UartStopBits stop_bits) {
  switch (stop_bits) {
  case UC_UART_STOP_BITS_2:
    return 2;
  case UC_UART_STOP_BITS_1:
  default:
    return 1;
  }
}

static lf_ret_t pico_uart_write(UartChannelCore* super, const unsigned char* data, size_t len) {
  UartPolledChannel* self = (UartPolledChannel*)super;
  // uart_write_blocking() returns void and spins until every byte is in the
  // FIFO, so there is no partial-write case to report.
  uart_write_blocking(self->dev, data, len);
  return LF_OK;
}

static void pico_uart_teardown(UartChannelCore* super) {
  UartPolledChannel* self = (UartPolledChannel*)super;
  uart_set_irq_enables(self->dev, false, false);
}

/* Runs in interrupt context. Drains the FIFO into the core's ring and wakes the
 * event loop. Framing and CRC happen later in poll(). */
static void pico_uart_isr(UartPolledChannel* self) {
  unsigned char buf[32];
  size_t n = 0;

  while (uart_is_readable(self->dev) && n < sizeof(buf)) {
    buf[n++] = (unsigned char)uart_getc(self->dev);
  }
  if (n > 0) {
    UartChannelCore_rx_push(&self->core, buf, n);
    UartChannelCore_notify();
  }
}

static void pico_uart_isr_0(void) {
  if (uart_channel_0 != NULL) {
    pico_uart_isr(uart_channel_0);
  }
}

static void pico_uart_isr_1(void) {
  if (uart_channel_1 != NULL) {
    pico_uart_isr(uart_channel_1);
  }
}

void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity_bits, UartStopBits stop_bits) {
  assert(self != NULL);

  int rx_pin;
  int tx_pin;

  if (uart_device == 0) {
    self->dev = uart0;
    uart_channel_0 = self;
    tx_pin = 0;
    rx_pin = 1;
  } else if (uart_device == 1) {
    self->dev = uart1;
    uart_channel_1 = self;
    tx_pin = 8;
    rx_pin = 9;
  } else {
    throw("The Raspberry Pi Pico only supports uart devices 0 and 1.");
    return;
  }

  uart_init(self->dev, 2400);
  gpio_set_function(tx_pin, UART_FUNCSEL_NUM(self->dev, tx_pin));
  gpio_set_function(rx_pin, UART_FUNCSEL_NUM(self->dev, rx_pin));

  // Undriven RX pin floats: it drifts to an indeterminate level and picks up
  // noise capacitively, and the UART reads the first falling edge as a start
  // bit and clocks in a byte of garbage. UART idle is high, so a pull-up makes
  // an unplugged or unpowered peer read as silence instead.
  gpio_pull_up(rx_pin);

  int actual = uart_set_baudrate(self->dev, baud);
  if (actual != (int)baud) {
    UART_PICO_WARN("Requested baud %u but got %d", baud, actual);
  }

  uart_set_hw_flow(self->dev, false, false);
  uart_set_format(self->dev, from_uc_data_bits(data_bits), from_uc_stop_bits(stop_bits),
                  from_uc_parity_bits(parity_bits));

  uart_set_fifo_enabled(self->dev, false);
  UartChannelCore_ctor(&self->core, pico_uart_write, pico_uart_teardown);

  if (uart_device == 0) {
    irq_set_exclusive_handler(UART0_IRQ, pico_uart_isr_0);
    irq_set_enabled(UART0_IRQ, true);
  } else {
    irq_set_exclusive_handler(UART1_IRQ, pico_uart_isr_1);
    irq_set_enabled(UART1_IRQ, true);
  }
  uart_set_irq_enables(self->dev, true, false);

  UART_PICO_INFO("Configured uart%u at %u baud", uart_device, baud);
}
