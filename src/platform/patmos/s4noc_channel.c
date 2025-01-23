#include "reactor-uc/platform/patmos/s4noc_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"
#include "machine/patmos.h"
#include <arpa/inet.h>

#define S4NOC_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)
#define S4NOC_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)
#define S4NOC_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)
#define S4NOC_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)

static lf_ret_t S4NOCPollChannel_open_connection(NetworkChannel *untyped_self) {
  S4NOC_CHANNEL_DEBUG("Open connection");
  (void)untyped_self;
  return LF_OK;
}

static void S4NOCPollChannel_close_connection(NetworkChannel *untyped_self) {
  S4NOC_CHANNEL_DEBUG("Close connection");
  (void)untyped_self;
}

static void S4NOCPollChannel_free(NetworkChannel *untyped_self) {
  S4NOC_CHANNEL_DEBUG("Free");
  (void)untyped_self;
}

static bool S4NOCPollChannel_is_connected(NetworkChannel *untyped_self) {
  S4NOCPollChannel *self = (S4NOCPollChannel *)untyped_self;

  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t S4NOCPollChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  S4NOCPollChannel *self = (S4NOCPollChannel *)untyped_self;

  volatile _IODEV int *s4noc_data = (volatile _IODEV int *)(PATMOS_IO_S4NOC + 4);
  volatile _IODEV int *s4noc_dest = (volatile _IODEV int *)(PATMOS_IO_S4NOC + 8);

  if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
    int message_size = serialize_message(message, self->write_buffer, S4NOC_CHANNEL_BUFFERSIZE);
    S4NOC_CHANNEL_DEBUG("Sending Message of Size: %i", message_size);

    *s4noc_dest = self->destination_core;
    int bytes_send = 0;
    while (bytes_send < message_size) {
      *s4noc_data = ((int *)self->write_buffer)[bytes_send / 4];
      bytes_send += 4;
    }

    return LF_OK;
  } else {
    return LF_ERR;
  }
}

static void S4NOCPollChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                       void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                                const FederateMessage *msg),
                                                       FederatedConnectionBundle *conn) {
  S4NOC_CHANNEL_INFO("Register receive callback");
  S4NOCPollChannel *self = (S4NOCPollChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

void S4NOCPollChannel_poll(NetworkChannel *untyped_self) {
  S4NOCPollChannel *self = (S4NOCPollChannel *)untyped_self;

  volatile _IODEV int *s4noc_status = (volatile _IODEV int *)PATMOS_IO_S4NOC;
  volatile _IODEV int *s4noc_data = (volatile _IODEV int *)(PATMOS_IO_S4NOC + 4);
  volatile _IODEV int *s4noc_source = (volatile _IODEV int *)(PATMOS_IO_S4NOC + 8);

  if (((*s4noc_status) & 0x02) == 0) {
    return;
  }

  int value = *s4noc_data;
  int source = *s4noc_source;

  S4NOCPollChannel *receive_channel = s4noc_global_state.core_channels[get_cpuid()][source];

  ((int *)receive_channel->receive_buffer)[receive_channel->receive_buffer_index / 4] = value;
  receive_channel->receive_buffer_index += 4;

  int expected_message_size = ntohl(*((int *)receive_channel->receive_buffer));
  if (expected_message_size + 4 > receive_channel->receive_buffer_index) {
    int bytes_left = deserialize_from_protobuf(&receive_channel->output, receive_channel->receive_buffer + 1,
                                               self->receive_buffer_index - 4);
    S4NOC_CHANNEL_DEBUG("Bytes Left after attempted to deserialize %d", bytes_left);

    if (bytes_left >= 0) {
      int receive_buffer_index = receive_channel->receive_buffer_index;
      receive_channel->receive_buffer_index = bytes_left;

      if (receive_channel->receive_callback != NULL) {
        S4NOC_CHANNEL_DEBUG("calling user callback!");
        receive_channel->receive_callback(self->federated_connection, &self->output);
      }
    }
  }
}

void S4NOCPollChannel_ctor(S4NOCPollChannel *self, Environment *env, unsigned int destination_core) {
  assert(self != NULL);
  assert(env != NULL);

  self->super.super.mode = NETWORK_CHANNEL_MODE_POLL;
  self->super.super.expected_connect_duration = SEC(0);
  self->super.super.type = NETWORK_CHANNEL_TYPE_S4NOC;
  self->super.super.is_connected = S4NOCPollChannel_is_connected;
  self->super.super.open_connection = S4NOCPollChannel_open_connection;
  self->super.super.close_connection = S4NOCPollChannel_close_connection;
  self->super.super.send_blocking = S4NOCPollChannel_send_blocking;
  self->super.super.register_receive_callback = S4NOCPollChannel_register_receive_callback;
  self->super.super.free = S4NOCPollChannel_free;
  self->super.poll = S4NOCPollChannel_poll;

  // Concrete fields
  self->receive_buffer_index = 0;
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
  self->destination_core = destination_core;
}
