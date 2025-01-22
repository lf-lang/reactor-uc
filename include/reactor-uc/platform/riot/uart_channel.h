#ifndef REACTOR_UC_UART_CHANNEL_H
#define REACTOR_UC_UART_CHANNEL_H

#define MODULE_PERIPH_UART_RXSTART_IRQ

#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include "periph/uart.h"
#include "cond.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct UARTSyncChannel UARTSyncChannel;
typedef struct UARTAsyncChannel UARTAsyncChannel;

#define UART_CHANNEL_BUFFERSIZE 1024
#define UART_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(10) // TODO:

struct UARTSyncChannel {
  SyncNetworkChannel super;
  NetworkChannelState state;

  FederateMessage output;
  unsigned char write_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned char read_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned int read_index;
  uart_t uart_dev;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

struct UARTAsyncChannel {
  UARTSyncChannel super;

  char connection_thread_stack[THREAD_STACKSIZE_MAIN];
  int connection_thread_pid;
  mutex_t receive_lock;
  cond_t receive_cv;
};

void UARTSyncChannel_ctor(UARTSyncChannel *self, Environment *env, uint32_t uart_device, uint32_t baud);

void UARTAsyncChannel_ctor(UARTAsyncChannel *self, Environment *env, uint32_t uart_device, uint32_t baud);

#endif
