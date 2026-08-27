#ifndef REACTOR_UC_RIOT_UART_CHANNEL_H
#define REACTOR_UC_RIOT_UART_CHANNEL_H

/**
 * @brief RIOT binding for the shared UART channel core.
 */

#include "reactor-uc/network_channel.h"

#include "cond.h"
#include "periph/uart.h"

struct UartPolledChannel {
  UartChannelCore core;
  uart_t uart_dev;
};

struct UartAsyncChannel {
  UartPolledChannel super;

  char decode_thread_stack[THREAD_STACKSIZE_MAIN];
  int decode_thread_pid;
  mutex_t receive_lock;
  cond_t receive_cv;
};

void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits);

void UartAsyncChannel_ctor(UartAsyncChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                           UartParityBits parity, UartStopBits stop_bits);

#endif // REACTOR_UC_RIOT_UART_CHANNEL_H
