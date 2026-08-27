#ifndef REACTOR_UC_UART_CHANNEL_H
#define REACTOR_UC_UART_CHANNEL_H

#include "reactor-uc/network_channel.h"
#include "reactor-uc/network_channel/frame.h"

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

#define UART_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(0)

// Room for two maximum-length frames, so a burst survives until poll() runs.
#ifndef UART_CORE_RX_RING_SIZE
#define UART_CORE_RX_RING_SIZE (2 * LF_FRAME_MAX_FRAME_SIZE)
#endif

/* Split from the definition below because the platform hooks at the bottom of
 * the struct take a `UartChannelCore*`: the typedef name has to exist before
 * the closing brace. */
typedef struct UartChannelCore UartChannelCore;

struct UartChannelCore {
  PolledNetworkChannel super;
  NetworkChannelState state;
  FederatedConnectionBundle* bundle;

  // Ring buffer. The ISR advances head, poll() advances tail.
  volatile unsigned int head;
  volatile unsigned int tail;
  unsigned char ring[UART_CORE_RX_RING_SIZE];

  /* Non-zero means the event loop is not draining fast enough. That is a
   * real-time fault, not a link fault. */
  uint32_t stat_ring_overflow;

  LfFrameReceiver rx;

  // Separate rx and tx payload buffers so the ISR can push a new frame while 
  // poll() is still processing the previous one. 
  FederateMessage output;
  unsigned char rx_payload[LF_FRAME_MAX_PAYLOAD];
  unsigned char tx_payload[LF_FRAME_MAX_PAYLOAD];
  unsigned char send_buffer[LF_FRAME_MAX_FRAME_SIZE];

  /** Callback to invoke when a frame is successfully decoded. */
  void (*receive_callback)(FederatedConnectionBundle* bundle, const FederateMessage* message);

  // What a platform must provide, everything else is in the core.
  /** Write @p len bytes, blocking until the hardware has accepted them all. */
  void (*write)(UartChannelCore* self, const unsigned char* data, size_t len);
  /** Release the device: stop interrupts, etc. */
  void (*teardown)(UartChannelCore* self);
};

/**
 * @brief Initialize a UartChannelCore. The platform must provide a write()
 * function and may provide a teardown() function. The core provides the rest
 * of the NetworkChannel API.
 */
void UartChannelCore_ctor(UartChannelCore* self,
                          void (*write)(UartChannelCore* self, const unsigned char* data, size_t len),
                          void (*teardown)(UartChannelCore* self));

/**
 * @brief Hand received bytes to the core. Safe to call from an ISR.
 *
 * Bytes that do not fit are dropped and counted in @ref stat_ring_overflow.
 * Dropping truncates the byte stream, which costs at most one frame because
 * COBS restores sync at the next delimiter.
 */
void UartChannelCore_rx_push(UartChannelCore* self, const unsigned char* data, size_t len);

/** @brief Wake the reactor event loop. Safe to call from an ISR. */
void UartChannelCore_notify(void);

#endif // REACTOR_UC_UART_CHANNEL_H