#include "reactor-uc/platform/riot/uart_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/serialization.h"
#include "led.h"

#define UART_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "UARTChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "UARTChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "UARTChannel: " fmt, ##__VA_ARGS__)
#define UART_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "UARTChannel: " fmt, ##__VA_ARGS__)

static lf_ret_t UARTChannel_open_connection(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Open connection");
  (void)untyped_self;
  return LF_OK;
}

static void UARTChannel_close_connection(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Close connection");
  (void)untyped_self;
}

static void UARTChannel_free(NetworkChannel *untyped_self) {
  UART_CHANNEL_DEBUG("Free");
  (void)untyped_self;
}

static bool UARTChannel_is_connected(NetworkChannel *untyped_self) {
  UARTChannel *self = (UARTChannel *)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t UARTChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  UARTChannel *self = (UARTChannel *)untyped_self;

  if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
    int message_size = serialize_to_protobuf(message, self->write_buffer, UART_CHANNEL_BUFFERSIZE);

    UART_CHANNEL_DEBUG("Sending Message of Size: %i", message_size);

    uart_write(self->uart_dev, self->write_buffer, message_size);
    return LF_OK;
  } else {
    return LF_ERR;
  }
}

static void UARTChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                  void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                           const FederateMessage *msg),
                                                  FederatedConnectionBundle *conn) {
  UART_CHANNEL_INFO("Register receive callback");
  UARTChannel *self = (UARTChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

void _UARTChannel_receive_callback(void *arg, uint8_t received_byte) {
  UARTChannel *self = (UARTChannel *)arg;
  const uint32_t minimum_message_size = 12;

  self->read_buffer[self->read_index] = received_byte;
  self->read_index++;

  if (self->read_index >= minimum_message_size) {
    cond_signal(&self->receive_cv);
  }
}

void *_UARTChannel_decode_loop(void *arg) {
  UARTChannel *self = (UARTChannel *)arg;
  mutex_lock(&self->receive_lock);

  UART_CHANNEL_DEBUG("Entering decoding loop");
  while (true) {
    cond_wait(&self->receive_cv, &self->receive_lock);

    int bytes_left = deserialize_from_protobuf(&self->output, self->read_buffer, self->read_index);
    UART_CHANNEL_DEBUG("Bytes Left after attempted to deserialize %d", bytes_left);

    if (bytes_left >= 0) {
      int read_index = self->read_index;
      self->read_index = bytes_left;

      memcpy(self->read_buffer, self->read_buffer + (read_index - bytes_left), bytes_left);

      if (self->receive_callback != NULL) {
        UART_CHANNEL_DEBUG("calling user callback!");
        self->receive_callback(self->federated_connection, &self->output);
      }
    }
  }

  return NULL;
}

void UARTChannel_ctor(UARTChannel *self, Environment *env, uint32_t baud) {
  assert(self != NULL);
  assert(env != NULL);

  self->uart_dev = UART_DEV(0);

  int result = uart_init(self->uart_dev, baud, _UARTChannel_receive_callback, self);

  if (result == -ENODEV) {
    UART_CHANNEL_ERR("Invalid UART device!");
  } else if (result == ENOTSUP) {
    UART_CHANNEL_ERR("Given configuration to uart is not supported!");
  } else if (result < 0) {
    UART_CHANNEL_ERR("UART Init error occurred");
  }

  result = uart_mode(self->uart_dev, UART_DATA_BITS_8, UART_PARITY_EVEN, UART_STOP_BITS_2);

  if (result != UART_OK) {
    UART_CHANNEL_ERR("Problem to configure UART device!");
    return;
  }

  cond_init(&self->receive_cv);
  mutex_init(&self->receive_lock);

  // Create decoding thread
  self->connection_thread_pid =
      thread_create(self->connection_thread_stack, sizeof(self->connection_thread_stack), THREAD_PRIORITY_MAIN - 1, 0,
                    _UARTChannel_decode_loop, self, "uart_channel_decode_loop");

  self->super.expected_connect_duration = UART_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.type = NETWORK_CHANNEL_TYPE_UART;
  self->super.is_connected = UARTChannel_is_connected;
  self->super.open_connection = UARTChannel_open_connection;
  self->super.close_connection = UARTChannel_close_connection;
  self->super.send_blocking = UARTChannel_send_blocking;
  self->super.register_receive_callback = UARTChannel_register_receive_callback;
  self->super.free = UARTChannel_free;

  // Concrete fields
  self->read_index = 0;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
}
