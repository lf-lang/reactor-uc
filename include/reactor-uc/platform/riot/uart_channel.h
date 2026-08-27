#ifndef REACTOR_UC_RIOT_UART_CHANNEL_H
#define REACTOR_UC_RIOT_UART_CHANNEL_H

/**
 * @brief RIOT binding for the shared UART channel core.
 */

#include "reactor-uc/network_channel.h"

#include "periph/uart.h"
#include "sema.h"

typedef struct UartPolledChannel UartPolledChannel;
typedef struct UartAsyncChannel UartAsyncChannel;

struct UartPolledChannel {
  UartChannelCore core;
  uart_t uart_dev;
};

struct UartAsyncChannel {
  UartPolledChannel super;

  char decode_thread_stack[THREAD_STACKSIZE_MAIN + LF_FRAME_MAX_PAYLOAD + 4];
  int decode_thread_pid;

  /* Counting, deliberately: a cond_signal with no waiter is dropped, so a frame
   * completing while the decode thread was still inside poll() would strand that
   * frame until some later frame happened to arrive. */
  sema_t frame_ready;
};

void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits);

void UartAsyncChannel_ctor(UartAsyncChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                           UartParityBits parity, UartStopBits stop_bits);

#endif // REACTOR_UC_RIOT_UART_CHANNEL_H
