#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"
#include <machine/patmos.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#define S4NOC_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)
#define S4NOC_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)
#define S4NOC_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)
#define S4NOC_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "S4NOCPollChannel: " fmt, ##__VA_ARGS__)

#if HANDLE_NEW_CONNECTIONS
#define S4NOC_OPEN_MESSAGE_REQUEST {0xC0, 0x18, 0x11, 0xC0}
#define S4NOC_OPEN_MESSAGE_RESPONSE {0xC0, 0xFF, 0x31, 0xC0}
#endif

S4NOCGlobalState s4noc_global_state = {0};

lf_ret_t S4NOCPollChannel_poll(NetworkChannel* untyped_self);

static void S4NOCPollChannel_close_connection(NetworkChannel* untyped_self) {
  S4NOC_CHANNEL_DEBUG("Close connection");
  S4NOCPollChannel* self = (S4NOCPollChannel*)untyped_self;
  self->state = NETWORK_CHANNEL_STATE_CLOSED;
}

static void S4NOCPollChannel_free(NetworkChannel* untyped_self) {
  S4NOC_CHANNEL_DEBUG("Free");
  (void)untyped_self;
}

static bool S4NOCPollChannel_is_connected(NetworkChannel* untyped_self) {
  S4NOCPollChannel* self = (S4NOCPollChannel*)untyped_self;
#if HANDLE_NEW_CONNECTIONS
  volatile _IODEV int* s4noc_data = (volatile _IODEV int*)(PATMOS_IO_S4NOC + 4);
  volatile _IODEV int* s4noc_dest = (volatile _IODEV int*)(PATMOS_IO_S4NOC + 8);

  if (!self->received_response) {
    *s4noc_dest = self->destination_core;
    uint8_t connect_message[] = S4NOC_OPEN_MESSAGE_REQUEST;
    uint32_t w = 0;
    memcpy(&w, connect_message, sizeof(w));
    S4NOC_CHANNEL_DEBUG("Open Connection - Sending ping");
    *s4noc_data = (int)w;
  }

  // poll if your channel is disconnected
  if (self->state != NETWORK_CHANNEL_STATE_CONNECTED) {
    S4NOCPollChannel_poll(untyped_self);
    S4NOC_CHANNEL_WARN("Channel is not connected (state=%d)", self->state);
  }

  S4NOC_CHANNEL_DEBUG("Network is %s", NetworkChannel_state_to_string(self->state));
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED && self->received_response && self->send_response;
#else
  S4NOC_CHANNEL_DEBUG("New connection handling disabled; assuming connected.");
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
#endif
}

static lf_ret_t S4NOCPollChannel_open_connection(NetworkChannel* untyped_self) {
  S4NOC_CHANNEL_DEBUG("Open connection");
  // S4NOCPollChannel *self = (S4NOCPollChannel *)untyped_self;
  // self->state = NETWORK_CHANNEL_STATE_CONNECTED;
  S4NOCPollChannel_is_connected(untyped_self);
  return LF_OK;
}

void print_buf(const char* header, const uint8_t* buf, size_t len) {
  S4NOC_CHANNEL_DEBUG("%s Buffer Length: %zu", header, len);
  char line[128];
  size_t offset = 0;
  offset += snprintf(line + offset, sizeof(line) - offset, "%s:", header);
  for (size_t i = 0; i < len; i++) {
    offset += snprintf(line + offset, sizeof(line) - offset, " %02X", buf[i]);
    if (offset >= sizeof(line) - 4) {
      S4NOC_CHANNEL_DEBUG("%s", line);
      offset = 0;
      offset += snprintf(line + offset, sizeof(line) - offset, "%s:", header);
    }
  }
  if (offset > 0) {
    S4NOC_CHANNEL_DEBUG("%s", line);
  }
}

void printf_msg(const char* header, const FederateMessage* message) {
  char buffer[128];
  int offset = 0;

  offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%d ", message->which_message);

  if (message->which_message == FederateMessage_clock_sync_msg_tag) {
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "ClockSyncMessage: ");

    switch (message->message.clock_sync_msg.which_message) {

    case ClockSyncMessage_priority_request_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "ClockPriorityRequest");
      break;

    case ClockSyncMessage_priority_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "ClockPriority: priority = %" PRIu64,
                         message->message.clock_sync_msg.message.priority.priority);
      break;

    case ClockSyncMessage_request_sync_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "RequestSync: seq = %" PRId32,
                         message->message.clock_sync_msg.message.request_sync.sequence_number);
      break;

    case ClockSyncMessage_sync_response_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "SyncResponse: seq = %" PRId32 ", time = %" PRIu64,
                         message->message.clock_sync_msg.message.sync_response.sequence_number,
                         message->message.clock_sync_msg.message.sync_response.time);
      break;

    case ClockSyncMessage_delay_request_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "DelayRequest: seq = %" PRId32,
                         message->message.clock_sync_msg.message.delay_request.sequence_number);
      break;

    case ClockSyncMessage_delay_response_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "DelayResponse: seq = %" PRId32 ", time = %" PRIu64,
                         message->message.clock_sync_msg.message.delay_response.sequence_number,
                         message->message.clock_sync_msg.message.delay_response.time);
      break;

    case ClockSyncMessage_priority_broadcast_request_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "ClockPriorityBroadcastRequest");
      break;

    default:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Unknown ClockSyncMessage type: %d",
                         message->message.clock_sync_msg.which_message);
      break;
    }

  } else if (message->which_message == FederateMessage_tagged_message_tag) {

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "TaggedMessage: tag = %" PRIu64 ", conn_id = %" PRId32,
                       message->message.tagged_message.tag.time, message->message.tagged_message.conn_id);

  } else if (message->which_message == FederateMessage_startup_coordination_tag) {

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "StartupCoordination: ");

    switch (message->message.startup_coordination.which_message) {

    case StartupCoordination_startup_handshake_request_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "StartupHandshakeRequest");
      break;

    case StartupCoordination_startup_handshake_response_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "StartupHandshakeResponse: state = %d",
                         message->message.startup_coordination.message.startup_handshake_response.state);
      break;

    case StartupCoordination_start_time_proposal_tag:
      offset +=
          snprintf(buffer + offset, sizeof(buffer) - offset, "StartTimeProposal: time = %" PRIu64 ", step = %" PRIu32,
                   message->message.startup_coordination.message.start_time_proposal.time,
                   message->message.startup_coordination.message.start_time_proposal.step);
      break;

    case StartupCoordination_start_time_response_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                         "StartTimeResponse: time = %" PRIu64 ", federation_start_time = %" PRIu64,
                         message->message.startup_coordination.message.start_time_response.elapsed_logical_time,
                         message->message.startup_coordination.message.start_time_response.federation_start_time);
      break;

    case StartupCoordination_start_time_request_tag:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "StartTimeRequest");
      break;

    default:
      offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Unknown StartupCoordination type: %d",
                         message->message.startup_coordination.which_message);
      break;
    }

  } else {
    offset +=
        snprintf(buffer + offset, sizeof(buffer) - offset, "Unknown FederateMessage type: %d", message->which_message);
  }
  S4NOC_CHANNEL_DEBUG("%s %s", header, buffer);
}

static lf_ret_t S4NOCPollChannel_send_blocking(NetworkChannel* untyped_self, const FederateMessage* message) {
  S4NOCPollChannel* self = (S4NOCPollChannel*)untyped_self;

  volatile _IODEV int* s4noc_data = (volatile _IODEV int*)(PATMOS_IO_S4NOC + 4);
  volatile _IODEV int* s4noc_dest = (volatile _IODEV int*)(PATMOS_IO_S4NOC + 8);
  S4NOC_CHANNEL_DEBUG("S4NOCPollChannel_send_blocking from core %d to core %d", get_cpuid(), self->destination_core);
  if (self->state == NETWORK_CHANNEL_STATE_CONNECTED) {
    // Print the FederateMessage type before sending
    printf_msg("sending msg type:", message);
    int message_size = serialize_to_protobuf(message, self->write_buffer + 4, S4NOC_CHANNEL_BUFFERSIZE - 4);

    uint32_t sz32 = (uint32_t)message_size;
    memcpy(self->write_buffer, &sz32, sizeof(sz32));
    // *((int *)self->write_buffer) = message_size;
    // S4NOC_CHANNEL_DEBUG("S4NOCPollChannel_send_blocking: message size: ((%d)).", message_size);
    int total_size = message_size + 4;
    S4NOC_CHANNEL_DEBUG("Total size to send: ((%d))", total_size);

    *s4noc_dest = self->destination_core;
    int bytes_send = 0;
    while (bytes_send < total_size) {
      // Wait for S4NOC to be ready (status bit 0x01 means ready to send)
      volatile _IODEV int* s4noc_status = (volatile _IODEV int*)PATMOS_IO_S4NOC;
      int retries = 0;
      while (((*s4noc_status) & 0x01) == 0) {
        // Busy-wait for S4NOC to be ready
        retries++;
        if (retries > 100000) {
          S4NOC_CHANNEL_ERR("S4NOC send timeout after %d retries, sent %d of %d bytes", retries, bytes_send,
                            total_size);
          return LF_ERR;
        }
      }
      int word_value = ((int*)self->write_buffer)[bytes_send / 4];
      *s4noc_data = word_value;
      bytes_send += 4;
      S4NOC_CHANNEL_DEBUG("Sent word %d (0x%08x), bytes_send now = %d of %d", (bytes_send / 4) - 1, word_value,
                          bytes_send, total_size);
    }
    S4NOC_CHANNEL_DEBUG("Completed sending ((%d)) bytes total", bytes_send);
    return LF_OK;
  } else {
    S4NOC_CHANNEL_ERR("Cannot send: Channel is not connected");
    return LF_ERR;
  }
}

static void S4NOCPollChannel_register_receive_callback(NetworkChannel* untyped_self,
                                                       void (*receive_callback)(FederatedConnectionBundle* conn,
                                                                                const FederateMessage* msg),
                                                       FederatedConnectionBundle* conn) {
  S4NOC_CHANNEL_INFO("Register receive callback at %p", receive_callback);
  S4NOCPollChannel* self = (S4NOCPollChannel*)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

lf_ret_t S4NOCPollChannel_poll(NetworkChannel* untyped_self) {
  S4NOCPollChannel* self = (S4NOCPollChannel*)untyped_self;
  volatile _IODEV int* s4noc_status = (volatile _IODEV int*)PATMOS_IO_S4NOC;
  volatile _IODEV int* s4noc_data = (volatile _IODEV int*)(PATMOS_IO_S4NOC + 4);
  volatile _IODEV int* s4noc_source = (volatile _IODEV int*)(PATMOS_IO_S4NOC + 8);

// if unconnected, and s4noc data available, respond.
#if HANDLE_NEW_CONNECTIONS
  if ((self->state != NETWORK_CHANNEL_STATE_CONNECTED) && (((*s4noc_status) & 0x02) != 0)) {
    uint32_t word = (uint32_t)(*s4noc_data);
    uint8_t req_bytes[] = S4NOC_OPEN_MESSAGE_REQUEST;
    uint8_t resp_bytes[] = S4NOC_OPEN_MESSAGE_RESPONSE;
    uint32_t req_word = 0;
    uint32_t resp_word = 0;
    memcpy(&req_word, req_bytes, sizeof(req_word));
    memcpy(&resp_word, resp_bytes, sizeof(resp_word));
    S4NOC_CHANNEL_DEBUG("Data available on S4NOC while channel is unconnected. word=0x%08x", word);
    if (word == req_word) {
      S4NOC_CHANNEL_INFO("Responding to S4NOC open message");
      *s4noc_data = (int)resp_word;
      *s4noc_source = self->destination_core;
      self->send_response = true;
    } else if (word == resp_word) {
      S4NOC_CHANNEL_INFO("Received S4NOC open message response");
      self->state = NETWORK_CHANNEL_STATE_CONNECTED;
      self->received_response = true;
    }

    return LF_OK;
  }
#endif
  // Check if data is available on the S4NOC interface
  if (((*s4noc_status) & 0x02) == 0) {
    S4NOC_CHANNEL_INFO("S4NOCPollChannel_poll: No data is available.");
    return LF_NETWORK_CHANNEL_EMPTY;
  }

  int value = *s4noc_data;
  int source = *s4noc_source;
  S4NOC_CHANNEL_INFO("S4NOCPollChannel_poll: Received data 0x%08x (%c%c%c%c) from source %d", value, ((char*)&value)[0],
                     ((char*)&value)[1], ((char*)&value)[2], ((char*)&value)[3], source);
  // Get the receive channel for the source core
  S4NOCPollChannel* receive_channel = s4noc_global_state.core_channels[source][get_cpuid()];
  S4NOC_CHANNEL_DEBUG("receive_channel pointer: %p, self pointer: %p", (void*)receive_channel, (void*)self);
  if (receive_channel == NULL) {
    S4NOC_CHANNEL_WARN("No receive_channel for source=%d dest=%d - dropping word", source, get_cpuid());
    return LF_ERR;
  }

  if (receive_channel->receive_buffer_index + 4 > S4NOC_CHANNEL_BUFFERSIZE) {
    S4NOC_CHANNEL_WARN("Receive buffer overflow: dropping message");
    receive_channel->receive_buffer_index = 0;
    return LF_ERR;
  }

  ((int*)receive_channel->receive_buffer)[receive_channel->receive_buffer_index / 4] = value;
  receive_channel->receive_buffer_index += 4;
  unsigned int expected_message_size = *((int*)receive_channel->receive_buffer);

  if (expected_message_size == 0 || expected_message_size > (S4NOC_CHANNEL_BUFFERSIZE - 4)) {
    S4NOC_CHANNEL_WARN(
        "Invalid expected message size: %u (receive_buffer_index=%d, buffer_size=%d). Dropping buffered data.",
        expected_message_size, receive_channel->receive_buffer_index, S4NOC_CHANNEL_BUFFERSIZE);
    receive_channel->receive_buffer_index = 0;
    memset(receive_channel->receive_buffer, 0, S4NOC_CHANNEL_BUFFERSIZE);
    return LF_ERR;
  }

  S4NOC_CHANNEL_DEBUG("Expected message size: %u, receive_buffer_index=%d", expected_message_size,
                      receive_channel->receive_buffer_index);
  if (receive_channel->receive_buffer_index >= expected_message_size + 4) {
    int bytes_left = deserialize_from_protobuf(&receive_channel->output,
                                               receive_channel->receive_buffer + 4, // skip the 4-byte size header
                                               expected_message_size                // only the message payload
    );
    S4NOC_CHANNEL_DEBUG("Bytes Left after attempted to deserialize: %d", bytes_left);

    // For federated messages on S4NoC we expect exactly one protobuf message per
    // frame and no trailing bytes. Treat any leftover bytes or parse error as a
    // stream error and drop the frame so we can resynchronize on the next one.
    if (bytes_left != 0) {
      if (bytes_left < 0) {
        S4NOC_CHANNEL_ERR("Error deserializing message (bytes_left=%d). Dumping %d received bytes for analysis:",
                          bytes_left, receive_channel->receive_buffer_index);
      } else {
        S4NOC_CHANNEL_ERR("Deserialization left %d unexpected trailing bytes. Dropping frame.", bytes_left);
      }
      print_buf("Dropped frame:", receive_channel->receive_buffer, receive_channel->receive_buffer_index);
      receive_channel->receive_buffer_index = 0;
      memset(receive_channel->receive_buffer, 0, S4NOC_CHANNEL_BUFFERSIZE);
      return LF_ERR;
    }

    // Successful parse with no trailing bytes.
    receive_channel->receive_buffer_index = 0;

    S4NOC_CHANNEL_DEBUG("Message received at core %d from core %d", get_cpuid(), source);
    printf_msg("received msg type:", &receive_channel->output);
    S4NOC_CHANNEL_INFO("Deserialization succeeded: bytes_left=%d, new receive_buffer_index=%d", bytes_left,
                       receive_channel->receive_buffer_index);
    if (receive_channel->receive_callback != NULL) {
      S4NOC_CHANNEL_DEBUG("calling user callback at %p!", receive_channel->receive_callback);
      receive_channel->receive_callback(self->federated_connection, &receive_channel->output);
      return LF_OK;
    } else {
      S4NOC_CHANNEL_WARN("No receive callback registered, dropping message");
      return LF_OK;
    }
  } else {
    S4NOC_CHANNEL_DEBUG("Message not complete yet: received %d of %d bytes", receive_channel->receive_buffer_index,
                        expected_message_size + 4);
  }
  return LF_NETWORK_CHANNEL_EMPTY;
}

void S4NOCPollChannel_ctor(S4NOCPollChannel* self, unsigned int destination_core) {
  assert(self != NULL);
  assert(destination_core < S4NOC_CORE_COUNT);

  S4NOC_CHANNEL_DEBUG("S4NOCPollChannel_ctor: destination_core=%d", destination_core);

  self->super.super.mode = NETWORK_CHANNEL_MODE_POLLED;
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
  self->state = HANDLE_NEW_CONNECTIONS ? NETWORK_CHANNEL_STATE_UNINITIALIZED : NETWORK_CHANNEL_STATE_CONNECTED;
  self->destination_core = destination_core;
  memset(self->receive_buffer, 0, S4NOC_CHANNEL_BUFFERSIZE);
  memset(self->write_buffer, 0, S4NOC_CHANNEL_BUFFERSIZE);
  unsigned int src_core = get_cpuid();
  s4noc_global_state.core_channels[src_core][destination_core] = self;
  for (int i = 0; i < S4NOC_CORE_COUNT; i++) {
    for (int j = 0; j < S4NOC_CORE_COUNT; j++) {
      if (s4noc_global_state.core_channels[i][j] != NULL) {
        S4NOC_CHANNEL_DEBUG("s4noc_global_state.core_channels[%d][%d] = %p", i, j,
                            (void*)s4noc_global_state.core_channels[i][j]);
      }
    }
  }
}
