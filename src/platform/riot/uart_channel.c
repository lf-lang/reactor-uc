#include "reactor-uc/platform/riot/uart_channel.h"
#include "reactor-uc/logging.h"

#include "thread.h"

#define UART_RIOT_ERR(fmt, ...) LF_ERR(NET, "UartChannel(riot): " fmt, ##__VA_ARGS__)
#define UART_RIOT_INFO(fmt, ...) LF_INFO(NET, "UartChannel(riot): " fmt, ##__VA_ARGS__)

static uart_data_bits_t from_uc_data_bits(UartDataBits data_bits) {
  switch (data_bits) {
  case UC_UART_DATA_BITS_5:
    return UART_DATA_BITS_5;
  case UC_UART_DATA_BITS_6:
    return UART_DATA_BITS_6;
  case UC_UART_DATA_BITS_7:
    return UART_DATA_BITS_7;
  case UC_UART_DATA_BITS_8:
  default:
    return UART_DATA_BITS_8;
  }
}

static uart_parity_t from_uc_parity_bits(UartParityBits parity_bits) {
  switch (parity_bits) {
  case UC_UART_PARITY_EVEN:
    return UART_PARITY_EVEN;
  case UC_UART_PARITY_ODD:
    return UART_PARITY_ODD;
  case UC_UART_PARITY_MARK:
    return UART_PARITY_MARK;
  case UC_UART_PARITY_SPACE:
    return UART_PARITY_SPACE;
  case UC_UART_PARITY_NONE:
  default:
    return UART_PARITY_NONE;
  }
}

static uart_stop_bits_t from_uc_stop_bits(UartStopBits stop_bits) {
  switch (stop_bits) {
  case UC_UART_STOP_BITS_2:
    return UART_STOP_BITS_2;
  case UC_UART_STOP_BITS_1:
  default:
    return UART_STOP_BITS_1;
  }
}

static lf_ret_t riot_uart_write(UartChannelCore* super, const unsigned char* data, size_t len) {
  UartPolledChannel* self = (UartPolledChannel*)super;
  // uart_write() returns void and blocks until every byte is out, so there is
  // no partial-write case to report.
  uart_write(self->uart_dev, data, len);
  return LF_OK;
}

// RIOT delivers received bytes one at a time from interrupt context.
// Both callbacks only buffer, decoding happens on the polling side.
static void riot_rx_polled(void* arg, uint8_t byte) {
  UartPolledChannel* self = (UartPolledChannel*)arg;
  if (UartChannelCore_rx_push(&self->core, &byte, 1)) {
    UartChannelCore_notify();
  }
}

static void riot_rx_async(void* arg, uint8_t byte) {
  UartAsyncChannel* self = (UartAsyncChannel*)arg;
  if (UartChannelCore_rx_push(&self->super.core, &byte, 1)) {
    // sema_post() only disables interrupts and bumps a counter, so it is safe here
    // and, unlike cond_signal(), cannot be lost when no thread is waiting yet.
    sema_post(&self->frame_ready);
  }
}

static void* riot_decode_loop(void* arg) {
  UartAsyncChannel* self = (UartAsyncChannel*)arg;
  NetworkChannel* chan = (NetworkChannel*)&self->super.core;

  UART_RIOT_INFO("Entering decode loop");
  while (true) {
    sema_wait(&self->frame_ready);
    // poll() drains the whole ring, so when several frames complete before this
    // thread runs the extra permits just cost one poll that finds nothing.
    ((PolledNetworkChannel*)chan)->poll(chan);
  }
  return NULL;
}

static void riot_uart_setup(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits, uart_rx_cb_t rx_cb, void* rx_arg) {
  self->uart_dev = UART_DEV(uart_device);

  int result = uart_init(self->uart_dev, baud, rx_cb, rx_arg);
  if (result == -ENODEV) {
    UART_RIOT_ERR("Invalid UART device %u", uart_device);
    throw("The user specified an invalid UART device!");
  } else if (result == ENOTSUP) {
    UART_RIOT_ERR("UART configuration not supported");
    throw("The given combination of parameters for creating a uart device is unsupported!");
  } else if (result < 0) {
    UART_RIOT_ERR("UART init error %d", result);
    throw("Unknown UART RIOT Error!");
  }

  result = uart_mode(self->uart_dev, from_uc_data_bits(data_bits), from_uc_parity_bits(parity),
                     from_uc_stop_bits(stop_bits));
  if (result != UART_OK) {
    UART_RIOT_ERR("uart_mode failed: %d", result);
    throw("RIOT was unable to configure the UART device!");
  }

  // No teardown: RIOT's uart_poweroff is optional and device-dependent.
  UartChannelCore_ctor(&self->core, riot_uart_write, NULL);
  UART_RIOT_INFO("Configured uart%u at %u baud", uart_device, baud);
}

void UartPolledChannel_ctor(UartPolledChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                            UartParityBits parity, UartStopBits stop_bits) {
  assert(self != NULL);
  riot_uart_setup(self, uart_device, baud, data_bits, parity, stop_bits, riot_rx_polled, self);
}

void UartAsyncChannel_ctor(UartAsyncChannel* self, uint32_t uart_device, uint32_t baud, UartDataBits data_bits,
                           UartParityBits parity, UartStopBits stop_bits) {
  assert(self != NULL);

  // Init BEFORE uart_init: the rx callback can fire as soon as the device is live,
  // and it posts this semaphore.
  sema_create(&self->frame_ready, 0);

  riot_uart_setup(&self->super, uart_device, baud, data_bits, parity, stop_bits, riot_rx_async, self);

  self->decode_thread_pid =
      thread_create(self->decode_thread_stack, sizeof(self->decode_thread_stack), THREAD_PRIORITY_MAIN - 1, 0,
                    riot_decode_loop, self, "uart_channel_decode_loop");

  self->super.core.super.super.mode = NETWORK_CHANNEL_MODE_ASYNC;
}
