#ifndef REACTOR_UC_UART_CHANNEL_H
#define REACTOR_UC_UART_CHANNEL_H

typedef enum UartDataBits UartDataBits;
typedef enum UartParityBits UartParityBits;
typedef enum UartStopBits UartStopBits;

enum UartDataBits { UC_UART_DATA_BITS_5, UC_UART_DATA_BITS_6, UC_UART_DATA_BITS_7, UC_UART_DATA_BITS_8 };

enum UartParityBits {
  UC_UART_PARITY_NONE,
  UC_UART_PARITY_EVEN,
  UC_UART_PARITY_ODD,
  UC_UART_PARITY_MARK,
  UC_UART_PARITY_SPACE
};

enum UartStopBits { UC_UART_STOP_BITS_1, UC_UART_STOP_BITS_2 };

#endif
