#ifndef REACTOR_UC_UART_CHANNEL_H
#define REACTOR_UC_UART_CHANNEL_H

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

/**
 * @brief Bytes an ISR moves per pass before handing them to the core.
 *
 * A stack scratch size, not a hardware property: 32 drains the RP2040's RX FIFO in
 * one pass and is comfortably more than nRF UARTE buffers, while staying small
 * enough to sit on an interrupt stack.
 */
#define UART_ISR_BURST_SIZE 32

/**
 * @brief Bookkeeping for one interrupt-driven transmit.
 *
 * Shared by every binding that hands a frame to a TX interrupt and sleeps until it
 * drains. @ref UartTxTransfer.len is the armed flag, and the ordering around it is
 * what makes the handover safe: it is written last when arming and cleared before
 * the waiter is released, so an ISR that observes it non-zero also observes a
 * consistent @ref UartTxTransfer.buf and @ref UartTxTransfer.off.
 */
typedef struct {
  const unsigned char* volatile buf;
  volatile size_t len; /**< Non-zero exactly while a transfer is armed. */
  volatile size_t off;
} UartTxTransfer;

/** @brief Put @p tx in the idle state. Call before the TX interrupt can fire. */
static inline void lf_uart_tx_init(UartTxTransfer* tx) {
  tx->buf = NULL;
  tx->off = 0;
  tx->len = 0;
}

/** @brief Arm @p tx for @p len bytes from @p data. Writes `len` last, by contract. */
static inline void lf_uart_tx_arm(UartTxTransfer* tx, const unsigned char* data, size_t len) {
  tx->buf = data;
  tx->off = 0;
  tx->len = len;
}

/** @brief True while a transfer is armed; only then may an ISR touch the buffer. */
static inline bool lf_uart_tx_armed(const UartTxTransfer* tx) { return tx->len > 0; }

/** @brief True once every byte has been handed to the hardware. */
static inline bool lf_uart_tx_complete(const UartTxTransfer* tx) { return tx->off >= tx->len; }

/** @brief Bytes still to hand over. */
static inline size_t lf_uart_tx_remaining(const UartTxTransfer* tx) { return tx->len - tx->off; }

/** @brief Clear the armed flag and report how many bytes made it to the hardware. */
static inline size_t lf_uart_tx_disarm(UartTxTransfer* tx) {
  const size_t sent = tx->off;
  tx->len = 0;
  return sent;
}

struct UartChannelCore {
  PolledNetworkChannel super;
  NetworkChannelState state;
  FederatedConnectionBundle* bundle;

  // Ring buffer. The ISR advances head, poll() advances tail.
  volatile unsigned int head;
  volatile unsigned int tail;
  volatile unsigned char ring[UART_CORE_RX_RING_SIZE];

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
  /**
   * @brief Write @p len bytes, blocking until the hardware has accepted them all.
   *
   * Whether that blocking is a busy-wait or a sleep is the binding's choice.
   * @return LF_OK when every byte reached the hardware, LF_ERR otherwise. A
   *         binding that gives up part-way must report LF_ERR: the peer sees a
   *         truncated frame, and send_blocking() has to pass that on rather
   *         than let the federated layer believe the message was delivered.
   */
  lf_ret_t (*write)(UartChannelCore* super, const unsigned char* data, size_t len);
  /** Release the device: stop interrupts, etc. */
  void (*teardown)(UartChannelCore* super);
};

/**
 * @brief Initialize a UartChannelCore. The platform must provide a write()
 * function and may provide a teardown() function. The core provides the rest
 * of the NetworkChannel API.
 */
void UartChannelCore_ctor(UartChannelCore* self,
                          lf_ret_t (*write)(UartChannelCore* super, const unsigned char* data, size_t len),
                          void (*teardown)(UartChannelCore* super));

/**
 * @brief Hand received bytes to the core. Safe to call from an ISR.
 *
 * Bytes that do not fit are dropped and counted in @ref stat_ring_overflow.
 * Dropping truncates the byte stream, which costs at most one frame because
 * COBS restores sync at the next delimiter.
 *
 * @return true when at least one frame delimiter was stored, i.e. poll() may now
 *         have a complete frame to hand up.
 */
bool UartChannelCore_rx_push(UartChannelCore* self, const unsigned char* data, size_t len);

/** @brief Wake the reactor event loop. Safe to call from an ISR. */
void UartChannelCore_notify(void);

/**
 * @brief How long a blocking write() should wait for @p len bytes to reach the hardware.
 *
 * Ten bit-times per byte is 8N1; this rounds up to twelve to cover parity plus two
 * stop bits, then multiplies by four so a burst of interrupt load or a briefly
 * stalled peer does not trip the timeout. The floor keeps short frames from timing
 * out on a slow link. A @p baud of 0 yields a one second fallback.
 *
 * Bindings that hand the frame to an interrupt and sleep need a bound so a peer that
 * stops accepting data cannot park the reactor thread forever.
 */
uint32_t lf_uart_tx_timeout_ms(uint32_t baud, size_t len);

#endif // REACTOR_UC_UART_CHANNEL_H
