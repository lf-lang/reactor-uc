#ifndef REACTOR_UC_UART_CHANNEL_H
#define REACTOR_UC_UART_CHANNEL_H

#define MODULE_PERIPH_UART_RXSTART_IRQ

#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include "periph/uart.h"
#include "cond.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct UARTPollChannel UARTPollChannel;
typedef struct UARTAsyncChannel UARTAsyncChannel;

#define UART_CHANNEL_BUFFERSIZE 1024
// The UartChannel is not connection-oriented and will always appear as connected, so no need to wait.
#define UART_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(0)

struct UARTPollChannel {
  SyncNetworkChannel super;
  NetworkChannelState state;
  Environment *env;

  FederateMessage output;
  unsigned char write_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned char receive_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned int receive_buffer_index;
  uart_t uart_dev;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

struct UARTAsyncChannel {
  UARTPollChannel super;

  char decode_thread_stack[THREAD_STACKSIZE_MAIN];
  int decode_thread_pid;
  mutex_t receive_lock;
  cond_t receive_cv;
};

void UARTPollChannel_ctor(UARTPollChannel *self, Environment *env, uint32_t uart_device, uint32_t baud,
                          uint8_t data_bits, uint8_t parity, uint8_t stop_bits);

void UARTAsyncChannel_ctor(UARTAsyncChannel *self, Environment *env, uint32_t uart_device, uint32_t baud,
                           uint8_t data_bits, uint8_t parity, uint8_t stop_bits);

#endif
