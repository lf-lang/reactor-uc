#ifndef REACTOR_UC_ZEPHYR_UART_CHANNEL_H
#define REACTOR_UC_ZEPHYR_UART_CHANNEL_H

/**
 * @brief Zephyr binding for the shared UART NetworkChannel core.
 */

#include "reactor-uc/network_channel.h"

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

typedef struct UartPolledChannel UartPolledChannel;

struct UartPolledChannel {
  UartChannelCore core;
  const struct device* dev;
  uint32_t baud; /**< Kept only to size the TX timeout. */

  /* Interrupt-driven TX, the same `uart_irq_*` API the RX path uses. write()
   * arms the transfer and blocks on `tx_done` while the TX interrupt drains
   * `tx_buf` into the hardware FIFO; `tx_len` doubles as the in-progress flag,
   * so the ISR only touches the buffer while it is non-zero. */
  const unsigned char* volatile tx_buf;
  volatile size_t tx_len;
  volatile size_t tx_off;
  struct k_sem tx_done;
};

/**
 * @param uart_device Index N, resolved to devicetree alias `lfuartN`.
 * @param baud        Baud rate. 1000000 is the design default.
 */
void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits);

#endif // REACTOR_UC_ZEPHYR_UART_CHANNEL_H
