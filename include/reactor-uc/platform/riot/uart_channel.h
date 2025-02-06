#ifndef REACTOR_UC_RIOT_UART_CHANNEL_H
#define REACTOR_UC_RIOT_UART_CHANNEL_H

#include "reactor-uc/network_channel/uart_channel.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include "periph/uart.h"
#include "cond.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct UartPolledChannel UartPolledChannel;
typedef struct UartAsyncChannel UartAsyncChannel;

#define UART_CHANNEL_BUFFERSIZE 1024
// The UartChannel is not connection-oriented and will always appear as connected, so no need to wait.
#define UART_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(0)

struct UartPolledChannel {
  PolledNetworkChannel super;
  NetworkChannelState state;

  FederateMessage output;
  unsigned char write_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned char receive_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned int receive_buffer_index;
  uart_t uart_dev;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

struct UartAsyncChannel {
  UartPolledChannel super;

  char decode_thread_stack[THREAD_STACKSIZE_MAIN];
  int decode_thread_pid;
  mutex_t receive_lock;
  cond_t receive_cv;
};

void UartPolledChannel_ctor(UartPolledChannel *self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits);

void UartAsyncChannel_ctor(UartAsyncChannel *self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                           UartParityBits parity, UartStopBits stop_bits);

#endif
