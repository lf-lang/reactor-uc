#ifndef REACTOR_UC_PICO_UART_CHANNEL_H
#define REACTOR_UC_PICO_UART_CHANNEL_H

#include "reactor-uc/network_channel/uart_channel.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct UartPolledChannel UartPolledChannel;
typedef struct UartAsyncChannel UartAsyncChannel;

#define UART_CHANNEL_BUFFERSIZE 1024

struct UartPolledChannel {
  PolledNetworkChannel super;
  NetworkChannelState state;
  FederateMessage output;
  unsigned char receive_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned char send_buffer[UART_CHANNEL_BUFFERSIZE];
  unsigned int receive_buffer_index;
  uart_inst_t *uart_device;

  FederatedConnectionBundle *bundle;
  void (*receive_callback)(FederatedConnectionBundle *bundle, const FederateMessage *message);
  bool send_response;
  bool received_response;
};

void UartPolledChannel_ctor(UartPolledChannel *self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits);

#endif
