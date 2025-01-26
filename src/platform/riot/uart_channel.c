#include "reactor-uc/platform/riot/uart_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"

#define UART_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "UARTPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "UARTPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "UARTPollChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "UARTPollChannel: " fmt, ##__VA_ARGS__)

static lf_ret_t UARTPollChannel_open_connection(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Open connection");
  (void)untyped_self;
  return LF_OK;
}

static void UARTPollChannel_close_connection(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Close connection");
  (void)untyped_self;
}

static void UARTPollChannel_free(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Free");
  (void)untyped_self;
}

static bool UARTPollChannel_is_connected(NetworkChannel *untyped_self) {
  UARTPollChannel *self = (UARTPollChannel *)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t UARTPollChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  UARTPollChannel *self = (UARTPollChannel *)untyped_self;

  if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
    int message_size = serialize_to_protobuf(message, self->write_buffer, UART_CHANNEL_BUFFERSIZE);

    UART_CHANNEL_DEBUG("Sending Message of Size: %i", message_size);

    uart_write(self->uart_dev, self->write_buffer, message_size);
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

static void UARTPollChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                      void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                               const FederateMessage *msg),
                                                      FederatedConnectionBundle *conn) {
  UART_CHANNEL_INFO("Register receive callback");
  UARTPollChannel *self = (UARTPollChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

void _UARTPollChannel_interrupt_callback(void *arg, uint8_t received_byte) {
  UARTPollChannel *self = (UARTPollChannel *)arg;
  const uint32_t minimum_message_size = 12;

  int receive_buffer_index = self->receive_buffer_index;
  self->receive_buffer_index++;

  self->receive_buffer[receive_buffer_index] = received_byte;

  if (self->receive_buffer_index >= minimum_message_size) {
    if (self->super.super.mode == NETWORK_CHANNEL_MODE_ASYNC) {
      cond_signal(&((UARTAsyncChannel *)self)->receive_cv);
    }
  }
}

void UARTPollChannel_poll(NetworkChannel *untyped_self) {
  UARTPollChannel *self = (UARTPollChannel *)untyped_self;
  const uint32_t minimum_message_size = 12;

  while (self->receive_buffer_index > minimum_message_size) {
    int bytes_left = deserialize_from_protobuf(&self->output, self->receive_buffer, self->receive_buffer_index);
    UART_CHANNEL_DEBUG("Bytes Left after attempted to deserialize %d", bytes_left);

    if (bytes_left >= 0) {

      self->env->enter_critical_section(self->env);
      int receive_buffer_index = self->receive_buffer_index;
      self->receive_buffer_index = bytes_left;
      self->env->leave_critical_section(self->env);

      memcpy(self->receive_buffer, self->receive_buffer + (receive_buffer_index - bytes_left), bytes_left);

      if (self->receive_callback != NULL) {
        UART_CHANNEL_DEBUG("calling user callback!");
        self->receive_callback(self->federated_connection, &self->output);
      }
    } else {
      break;
    }
  }
}

void *_UARTAsyncChannel_decode_loop(void *arg) {
  UARTAsyncChannel *self = (UARTAsyncChannel *)arg;
  mutex_lock(&self->receive_lock);

  UART_CHANNEL_DEBUG("Entering decoding loop");
  while (true) {
    cond_wait(&self->receive_cv, &self->receive_lock);

    UARTPollChannel_poll((NetworkChannel *)&self->super);
  }

  return NULL;
}

uart_data_bits_t from_uc_data_bits(UARTDataBits data_bits) {
  switch (data_bits) {
  case UC_UART_DATA_BITS_5:
    return UART_DATA_BITS_5;
  case UC_UART_DATA_BITS_6:
    return UART_DATA_BITS_6;
  case UC_UART_DATA_BITS_7:
    return UART_DATA_BITS_7;
  case UC_UART_DATA_BITS_8:
    return UART_DATA_BITS_8;
  };

  return UART_DATA_BITS_8;
}

uart_parity_t from_uc_parity_bits(UARTParityBits parity_bits) {
  switch (parity_bits) {
  case UC_UART_PARITY_NONE:
    return UART_PARITY_NONE;
  case UC_UART_PARITY_EVEN:
    return UART_PARITY_EVEN;
  case UC_UART_PARITY_ODD:
    return UART_PARITY_ODD;
  case UC_UART_PARITY_MARK:
    return UART_PARITY_MARK;
  case UC_UART_PARITY_SPACE:
    return UART_PARITY_SPACE;
  }

  return UART_PARITY_EVEN;
}

uart_stop_bits_t from_uc_stop_bits(UARTStopBits stop_bits) {
  switch (stop_bits) {
  case UC_UART_STOP_BITS_1:
    return UART_STOP_BITS_1;
  case UC_UART_STOP_BITS_2:
    return UART_STOP_BITS_2;
  }

  return UART_STOP_BITS_2;
}

void UARTPollChannel_ctor(UARTPollChannel *self, Environment *env, uint32_t uart_device, uint32_t baud,
                          UARTDataBits data_bits, UARTParityBits parity_bits, UARTStopBits stop_bits) {

  assert(self != NULL);
  assert(env != NULL);

  // Concrete fields
  self->receive_buffer_index = 0;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
  self->env = env;

  self->super.super.mode = NETWORK_CHANNEL_MODE_POLL;
  self->super.super.expected_connect_duration = UART_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.super.type = NETWORK_CHANNEL_TYPE_UART;
  self->super.super.is_connected = UARTPollChannel_is_connected;
  self->super.super.open_connection = UARTPollChannel_open_connection;
  self->super.super.close_connection = UARTPollChannel_close_connection;
  self->super.super.send_blocking = UARTPollChannel_send_blocking;
  self->super.super.register_receive_callback = UARTPollChannel_register_receive_callback;
  self->super.super.free = UARTPollChannel_free;
  self->super.poll = UARTPollChannel_poll;

  self->uart_dev = UART_DEV(uart_device);

  int result = uart_init(self->uart_dev, baud, _UARTPollChannel_interrupt_callback, self);

  if (result == -ENODEV) {
    UART_CHANNEL_ERR("Invalid UART device!");
    throw("The user specified an invalid UART device!");
  } else if (result == ENOTSUP) {
    UART_CHANNEL_ERR("Given configuration to uart is not supported!");
    throw("The given combination of parameters for creating a uart device is unsupported!");
  } else if (result < 0) {
    UART_CHANNEL_ERR("UART Init error occurred");
    throw("Unknown UART RIOT Error!");
  }

  result = uart_mode(self->uart_dev, from_uc_data_bits(data_bits), from_uc_parity_bits(parity_bits),
                     from_uc_stop_bits(stop_bits));

  if (result != UART_OK) {
    UART_CHANNEL_ERR("Problem to configure UART device!");
    throw("RIOT was unable to configure the UART device!");
  }
}

void UARTAsyncChannel_ctor(UARTAsyncChannel *self, Environment *env, uint32_t uart_device, uint32_t baud,
                           UARTDataBits data_bits, UARTParityBits parity, UARTStopBits stop_bits) {

  UARTPollChannel_ctor(&self->super, env, uart_device, baud, data_bits, parity, stop_bits);

  cond_init(&self->receive_cv);
  mutex_init(&self->receive_lock);

  // Create decoding thread
  self->decode_thread_pid =
      thread_create(self->decode_thread_stack, sizeof(self->decode_thread_stack), THREAD_PRIORITY_MAIN - 1, 0,
                    _UARTAsyncChannel_decode_loop, self, "uart_channel_decode_loop");

  self->super.super.super.mode = NETWORK_CHANNEL_MODE_ASYNC;
}
