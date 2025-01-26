#ifndef REACTOR_UC_UART_CHANNEL_H
#define REACTOR_UC_UART_CHANNEL_H

typedef enum UARTDataBits UARTDataBits;
typedef enum UARTParityBits UARTParityBits;
typedef enum UARTStopBits UARTStopBits;

enum UARTDataBits { UC_UART_DATA_BITS_5, UC_UART_DATA_BITS_6, UC_UART_DATA_BITS_7, UC_UART_DATA_BITS_8 };

enum UARTParityBits {
  UC_UART_PARITY_NONE,
  UC_UART_PARITY_EVEN,
  UC_UART_PARITY_ODD,
  UC_UART_PARITY_MARK,
  UC_UART_PARITY_SPACE
};

enum UARTStopBits { UC_UART_STOP_BITS_1, UC_UART_STOP_BITS_2 };

#endif
