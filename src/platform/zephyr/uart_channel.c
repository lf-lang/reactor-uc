#include "reactor-uc/platform/zephyr/uart_channel.h"
#include "reactor-uc/logging.h"

#include <zephyr/irq.h>
#include <zephyr/version.h>

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

  k_sem_reset(&self->tx_done);
  lf_uart_tx_arm(&self->tx, data, len);
  uart_irq_tx_enable(self->dev);

  if (k_sem_take(&self->tx_done, K_MSEC(lf_uart_tx_timeout_ms(self->baud, len))) != 0) {
    const unsigned int key = irq_lock();
    uart_irq_tx_disable(self->dev);
    const size_t sent = lf_uart_tx_disarm(&self->tx);
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
  unsigned char buf[UART_ISR_BURST_SIZE];

#if KERNELVERSION >= 0x04040000
  uart_irq_update(dev);
#else
  if (!uart_irq_update(dev)) {
    return;
  }
#endif

  bool got_frame = false;
  while (uart_irq_rx_ready(dev)) {
    const int n = uart_fifo_read(dev, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    if (UartChannelCore_rx_push(&self->super, buf, (size_t)n)) {
      got_frame = true;
    }
  }

  if (uart_irq_tx_ready(dev)) {
    if (!lf_uart_tx_armed(&self->tx)) {
      // Nothing armed: either a spurious ready, or write() gave up and already
      // disarmed. Leaving the interrupt enabled here would spin the CPU.
      uart_irq_tx_disable(dev);
    } else {
      if (!lf_uart_tx_complete(&self->tx)) {
        const int n = uart_fifo_fill(dev, &self->tx.buf[self->tx.off], (int)lf_uart_tx_remaining(&self->tx));
        if (n > 0) {
          self->tx.off += (size_t)n;
        }
      }
      if (lf_uart_tx_complete(&self->tx)) {
        // Every byte is in the FIFO, so write()'s buffer is free to reuse even
        // though the shift register has not drained yet.
        uart_irq_tx_disable(dev);
        (void)lf_uart_tx_disarm(&self->tx);
        k_sem_give(&self->tx_done);
      }
    }
  }

  if (got_frame) {
    UartChannelCore_notify();
  }
}

void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits) {
  assert(self != NULL);

  self->dev = uart_device_from_index(uart_device);
  self->baud = baud;

  // Before the first uart_irq_*_enable() below, so the ISR never sees an
  // uninitialised semaphore or a stale TX transfer.
  lf_uart_tx_init(&self->tx);
  k_sem_init(&self->tx_done, 0, 1);

  // Construct the core up front so every early return below still leaves a usable
  // vtable behind; `state` is what marks the channel unusable.
  UartChannelCore_ctor(&self->super, zephyr_uart_write, zephyr_uart_teardown);

  if (self->dev == NULL) {
    UART_ZEPHYR_ERR("No devicetree alias lfuart%u; add it to the board overlay", uart_device);
    self->super.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
    return;
  }
  if (!device_is_ready(self->dev)) {
    UART_ZEPHYR_ERR("UART device lfuart%u is not ready", uart_device);
    self->super.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
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
  if (ret == -ENOSYS || ret == -ENOTSUP) {
    /* The driver will not reconfigure this port at runtime. Zephyr's nRF54L
     * UARTE returns -ENOTSUP rather than -ENOSYS for this, even when every
     * individual parameter is supported. That is survivable, because the
     * devicetree already configured the port at boot, but only if the
     * overlay actually says what LF asked for, so check instead of assuming. */
    struct uart_config actual;
    if (uart_config_get(self->dev, &actual) != 0) {
      UART_ZEPHYR_WARN("uart_configure unsupported (%d) and current settings are "
                       "unreadable; trusting the overlay. Verify current-speed = <%u> "
                       "and that hw-flow-control is absent.",
                       ret, (unsigned)cfg.baudrate);
    } else if (actual.baudrate != cfg.baudrate || actual.parity != cfg.parity || actual.stop_bits != cfg.stop_bits ||
               actual.data_bits != cfg.data_bits) {
      UART_ZEPHYR_ERR("uart_configure unsupported (%d) and the devicetree does not match "
                      "the program: overlay gives baud=%u parity=%d stop=%d data=%d, LF "
                      "asked for baud=%u parity=%d stop=%d data=%d. Set current-speed in "
                      "the board overlay to match @interface_uart.",
                      ret, (unsigned)actual.baudrate, (int)actual.parity, (int)actual.stop_bits, (int)actual.data_bits,
                      (unsigned)cfg.baudrate, (int)cfg.parity, (int)cfg.stop_bits, (int)cfg.data_bits);
      self->super.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
      return;
    } else {
      UART_ZEPHYR_INFO("uart_configure unsupported (%d); devicetree settings match the "
                       "program, continuing.",
                       ret);
    }
  } else if (ret != 0) {
    UART_ZEPHYR_ERR("uart_configure failed: %d", ret);
    self->super.state = NETWORK_CHANNEL_STATE_UNINITIALIZED;
    return;
  }

  uart_irq_callback_user_data_set(self->dev, zephyr_uart_isr, self);
  uart_irq_rx_enable(self->dev);

  UART_ZEPHYR_INFO("Configured lfuart%u at %u baud, flow control %s", uart_device, baud,
                   cfg.flow_ctrl == UART_CFG_FLOW_CTRL_NONE ? "off" : "on");
}
