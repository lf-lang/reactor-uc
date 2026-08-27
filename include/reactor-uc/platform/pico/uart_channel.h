#ifndef REACTOR_UC_PICO_UART_CHANNEL_H
#define REACTOR_UC_PICO_UART_CHANNEL_H

/**
 * @brief Raspberry Pi Pico (pico-sdk) binding for the shared UART channel core.
 */

#include "reactor-uc/network_channel.h"

#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/sync.h"

typedef struct UartPolledChannel UartPolledChannel;

struct UartPolledChannel {
  UartChannelCore core;
  uart_inst_t* dev;
  uint32_t baud; /**< Kept only to size the TX timeout. */

  /* Interrupt-driven TX: write() primes the FIFO, arms TXIM and then sleeps on
   * `tx_done` while the interrupt drains the rest of `tx`. */
  UartTxTransfer tx;
  semaphore_t tx_done;
};

void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits);

#endif // REACTOR_UC_PICO_UART_CHANNEL_H
