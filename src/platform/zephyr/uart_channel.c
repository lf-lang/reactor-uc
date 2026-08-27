#include "reactor-uc/platform/zephyr/uart_channel.h"
#include "reactor-uc/logging.h"

#include <zephyr/irq.h>

#define UART_ZEPHYR_ERR(fmt, ...) LF_ERR(NET, "UartChannel(zephyr): " fmt, ##__VA_ARGS__)
#define UART_ZEPHYR_WARN(fmt, ...) LF_WARN(NET, "UartChannel(zephyr): " fmt, ##__VA_ARGS__)
#define UART_ZEPHYR_INFO(fmt, ...) LF_INFO(NET, "UartChannel(zephyr): " fmt, ##__VA_ARGS__)

// Map the LF `uart_device` index onto a devicetree alias.
// The application overlay defines `lfuart0`/`lfuart1`, anything else resolves
// to NULL and is reported at construction.
static const struct device* uart_device_from_index(uint32_t index) {
  switch (index) {
#if DT_NODE_EXISTS(DT_ALIAS(lfuart0))
  case 0:
    return DEVICE_DT_GET(DT_ALIAS(lfuart0));
#endif
#if DT_NODE_EXISTS(DT_ALIAS(lfuart1))
  case 1:
    return DEVICE_DT_GET(DT_ALIAS(lfuart1));
#endif
  default:
    return NULL;
  }
}

static enum uart_config_data_bits to_zephyr_data_bits(UartDataBits bits) {
  switch (bits) {
  case UC_UART_DATA_BITS_5:
    return UART_CFG_DATA_BITS_5;
  case UC_UART_DATA_BITS_6:
    return UART_CFG_DATA_BITS_6;
  case UC_UART_DATA_BITS_7:
    return UART_CFG_DATA_BITS_7;
  case UC_UART_DATA_BITS_8:
  default:
    return UART_CFG_DATA_BITS_8;
  }
}

static enum uart_config_parity to_zephyr_parity(UartParityBits parity) {
  switch (parity) {
  case UC_UART_PARITY_EVEN:
    return UART_CFG_PARITY_EVEN;
  case UC_UART_PARITY_ODD:
    return UART_CFG_PARITY_ODD;
  case UC_UART_PARITY_MARK:
    return UART_CFG_PARITY_MARK;
  case UC_UART_PARITY_SPACE:
    return UART_CFG_PARITY_SPACE;
  case UC_UART_PARITY_NONE:
  default:
    return UART_CFG_PARITY_NONE;
  }
}

static enum uart_config_stop_bits to_zephyr_stop_bits(UartStopBits bits) {
  switch (bits) {
  case UC_UART_STOP_BITS_2:
    return UART_CFG_STOP_BITS_2;
  case UC_UART_STOP_BITS_1:
  default:
    return UART_CFG_STOP_BITS_1;
  }
}

// How long write() waits for the TX interrupt to hand a frame to the hardware.
// Ten bit-times per byte is 8N1, round up to twelve to cover parity plus two stop
// bits, then multiply by four so a burst of interrupt load or a briefly deasserted
// CTS does not trip the timeout. The floor keeps short frames from timing out on a
// slow link.
static k_timeout_t zephyr_uart_tx_timeout(const UartPolledChannel* self, size_t len) {
  if (self->baud == 0) {
    return K_MSEC(1000);
  }
  const uint64_t wire_ms = ((uint64_t)len * 12ULL * 1000ULL) / self->baud;
  return K_MSEC((uint32_t)(wire_ms * 4ULL) + 10U);
}

// Arms the transfer and sleeps until the TX interrupt has pushed every byte into
// the FIFO. Must run in thread context: k_sem_take() with a timeout is illegal in
// an ISR. The core only calls this from send_blocking().
static lf_ret_t zephyr_uart_write(UartChannelCore* super, const unsigned char* data, size_t len) {
  UartPolledChannel* self = (UartPolledChannel*)super;
  if (self->dev == NULL) {
    return LF_ERR; // Construction failed.
  }
  if (len == 0) {
    return LF_OK;
  }

  self->tx_buf = data;
  self->tx_off = 0;
  k_sem_reset(&self->tx_done);
  self->tx_len = len; // Arms the ISR, so it must be written last.
  uart_irq_tx_enable(self->dev);

  if (k_sem_take(&self->tx_done, zephyr_uart_tx_timeout(self, len)) != 0) {
    const unsigned int key = irq_lock();
    uart_irq_tx_disable(self->dev);
    const size_t sent = self->tx_off;
    self->tx_len = 0;
    irq_unlock(key);
    // Giving up mid-frame truncates it, which costs the peer one frame.
    UART_ZEPHYR_ERR("TX timed out after %zu of %zu bytes; peer not draining the line", sent, len);
    return LF_ERR;
  }
  return LF_OK;
}

static void zephyr_uart_teardown(UartChannelCore* super) {
  UartPolledChannel* self = (UartPolledChannel*)super;
  if (self->dev != NULL) {
    uart_irq_rx_disable(self->dev);
    uart_irq_tx_disable(self->dev);
  }
}

// Drains the hardware RX FIFO into the core's ring, refills the TX FIFO from
// the frame write() handed over, and wakes the event loop.
// Decoding and CRC happen in poll().
static void zephyr_uart_isr(const struct device* dev, void* user_data) {
  UartPolledChannel* self = (UartPolledChannel*)user_data;
  unsigned char buf[32];

  if (!uart_irq_update(dev)) {
    return;
  }

  bool got_data = false;
  while (uart_irq_rx_ready(dev)) {
    const int n = uart_fifo_read(dev, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    UartChannelCore_rx_push(&self->core, buf, (size_t)n);
    got_data = true;
  }

  if (uart_irq_tx_ready(dev)) {
    if (self->tx_len == 0) {
      // Nothing armed: either a spurious ready, or write() gave up and already
      // disarmed. Leaving the interrupt enabled here would spin the CPU.
      uart_irq_tx_disable(dev);
    } else {
      if (self->tx_off < self->tx_len) {
        const int n = uart_fifo_fill(dev, &self->tx_buf[self->tx_off], (int)(self->tx_len - self->tx_off));
        if (n > 0) {
          self->tx_off += (size_t)n;
        }
      }
      if (self->tx_off >= self->tx_len) {
        // Every byte is in the FIFO, so write()'s buffer is free to reuse even
        // though the shift register has not drained yet.
        uart_irq_tx_disable(dev);
        self->tx_len = 0;
        k_sem_give(&self->tx_done);
      }
    }
  }

  if (got_data) {
    UartChannelCore_notify();
  }
}

void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits) {
  assert(self != NULL);

  self->core.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
  self->dev = uart_device_from_index(uart_device);
  self->baud = baud;

  // Before the first uart_irq_*_enable() below, so the ISR never sees an
  // uninitialised semaphore or a stale TX transfer.
  self->tx_buf = NULL;
  self->tx_len = 0;
  self->tx_off = 0;
  k_sem_init(&self->tx_done, 0, 1);

  if (self->dev == NULL) {
    UART_ZEPHYR_ERR("No devicetree alias lfuart%u; add it to the board overlay", uart_device);
    UartChannelCore_ctor(&self->core, zephyr_uart_write, zephyr_uart_teardown);
    self->core.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
    return;
  }
  if (!device_is_ready(self->dev)) {
    UART_ZEPHYR_ERR("UART device lfuart%u is not ready", uart_device);
    UartChannelCore_ctor(&self->core, zephyr_uart_write, zephyr_uart_teardown);
    self->core.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
    return;
  }

  /* Flow control is a board-wiring property, not something LF specifies: RTS/CTS
   * needs pins declared in the devicetree, and demanding it on a 2-wire link
   * makes uart_configure() fail outright. Keep whatever the overlay set up, so a
   * plain TX/RX link behaves the same here as it does on pico and riot, neither
   * of which uses flow control. */
  struct uart_config cfg = {
      .baudrate = baud,
      .parity = to_zephyr_parity(parity),
      .stop_bits = to_zephyr_stop_bits(stop_bits),
      .data_bits = to_zephyr_data_bits(data_bits),
      .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
  };

  struct uart_config current;
  if (uart_config_get(self->dev, &current) == 0) {
    cfg.flow_ctrl = current.flow_ctrl;
  }

  int ret = uart_configure(self->dev, &cfg);
  if (ret == -ENOSYS) {
    UART_ZEPHYR_WARN("uart_configure unsupported; using devicetree settings. "
                     "Verify current-speed and hw-flow-control in the overlay.");
  } else if (ret != 0) {
    UART_ZEPHYR_ERR("uart_configure failed: %d", ret);
    UartChannelCore_ctor(&self->core, zephyr_uart_write, zephyr_uart_teardown);
    self->core.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
    return;
  }

  UartChannelCore_ctor(&self->core, zephyr_uart_write, zephyr_uart_teardown);

  uart_irq_callback_user_data_set(self->dev, zephyr_uart_isr, self);
  uart_irq_rx_enable(self->dev);

  UART_ZEPHYR_INFO("Configured lfuart%u at %u baud, flow control %s", uart_device, baud,
                   cfg.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ? "off" : "on");
}
