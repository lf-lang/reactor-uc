#ifndef REACTOR_UC_PICO_UART_CHANNEL_H
#define REACTOR_UC_PICO_UART_CHANNEL_H

#include "reactor-uc/network_channel/uart_channel.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct UartPollChannel UartPollChannel;
typedef struct UartAsyncChannel UartAsyncChannel;

#define UART_CHANNEL_BUFFERSIZE 1024
// The UartChannel is not connection-oriented and will always appear as connected, so no need to wait.
#define UART_CHANNEL_EXPECTED_CONNECT_DURATION SEC(5)

extern UartPollChannel *uart_channel_0;
extern UartPollChannel *uart_channel_1;

struct UartPollChannel {
  PolledNetworkChannel super;

  unsigned char receive_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned int receive_buffer_index;
  uart_inst_t *uart_device;

  EncryptionLayer *encryption_layer;
  void (*receive_callback)(EncryptionLayer *encryption_layer, const char *message, ssize_t message_size);
};

void UartPollChannel_ctor(UartPollChannel *self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                          UartParityBits parity, UartStopBits stop_bits);

#endif
