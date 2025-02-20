#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/error.h"
#include "reactor-uc/logging.h"
#include "proto/message.pb.h"

#define UNKNOWN_PRIORITY INT32_MAX
#define CLOCK_SYNC_PERIOD SEC(1)
#define SERVO_KP 0.7              // Default value from linuxptp
#define SERVO_KI 0.3              // Default value from linuxptp
#define SERVO_CLAMP_PPB 200000000 // This is the default max-ppb value for linuxptp
#define NUM_RESERVED_EVENTS 1     // 1 event is reserved for the periodic event driving the clock sync.

static void ClockSynchronization_correct_clock(ClockSynchronization *self, ClockSyncTimestamps *timestamps) {
  LF_DEBUG(CLOCK_SYNC, "Correcting clock. T1=" PRINTF_TIME " T2=" PRINTF_TIME " T3=" PRINTF_TIME " T4=" PRINTF_TIME,
           timestamps->t1, timestamps->t2, timestamps->t3, timestamps->t4);
  interval_t rtt = (timestamps->t4 - timestamps->t1) - (timestamps->t3 - timestamps->t2);
  interval_t owd = rtt / 2;
  interval_t clock_offset = owd - (timestamps->t2 - timestamps->t1);
  LF_DEBUG(CLOCK_SYNC, "RTT: " PRINTF_TIME " OWD: " PRINTF_TIME " offset: " PRINTF_TIME, rtt, owd, clock_offset);

  self->servo.last_error = clock_offset;
  self->servo.accumulated_error += clock_offset;
  float correction_float = self->servo.Kp * clock_offset + self->servo.Ki * self->servo.accumulated_error;
  interval_t correction = (interval_t)correction_float;

  LF_DEBUG(CLOCK_SYNC, "Accumulated error: " PRINTF_TIME " Correction: " PRINTF_TIME, self->servo.accumulated_error,
           correction);
  if (correction > self->servo.clamp) {
    correction = self->servo.clamp;
  } else if (correction < -self->servo.clamp) {
    correction = -self->servo.clamp;
  }

  self->env->clock.adjust_time(&self->env->clock, correction);
}

static void ClockSynchronization_schedule_system_event(ClockSynchronization *self, instant_t time, int message_type) {
  ClockSyncEvent *payload = NULL;
  lf_ret_t ret;
  ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void **)&payload);
  if (ret != LF_OK) {
    LF_ERR(CLOCK_SYNC, "Failed to allocate payload for clock-sync system event.");
    validate(false);
    return;
  }
  payload->neighbor_index = -1; // -1 means that it is from ourself
  payload->msg.which_message = message_type;
  tag_t tag = {.time = time, .microstep = 0};
  SystemEvent event = SYSTEM_EVENT_INIT(tag, &self->super, (void *)payload);
  // TODO: Critical section?

  ret = self->env->scheduler->schedule_at_locked(self->env->scheduler, &event.super);
  if (ret != LF_OK) {
    LF_ERR(CLOCK_SYNC, "Failed to schedule clock-sync system event.");
    validate(false);
  }
}

static int ClockSynchronization_my_priority(ClockSynchronization *self) {
  if (self->is_grandmaster) {
    return 0;
  }

  int my_priority = UNKNOWN_PRIORITY;
  for (size_t i = 0; i < self->num_neighbours; i++) {
    int neighbor_priority = self->neighbor_clock[i].priority;
    if (neighbor_priority != UNKNOWN_PRIORITY && (neighbor_priority + 1) < my_priority) {
      my_priority = neighbor_priority + 1;
    }
  }
  return my_priority;
}

static void ClockSynchronization_handle_message_callback(ClockSynchronization *self, const ClockSyncMessage *msg,
                                                         size_t bundle_idx) {
  (void)self;
  (void)msg;
  (void)bundle_idx;
  LF_DEBUG(CLOCK_SYNC, "Received clock sync message from neighbor %zu. Scheduling as a system event", bundle_idx);
  ClockSyncEvent *payload = NULL;
  lf_ret_t ret = self->super.payload_pool.allocate_with_reserved(&self->super.payload_pool, (void **)&payload,
                                                                 NUM_RESERVED_EVENTS);
  if (ret == LF_OK) {
    payload->neighbor_index = bundle_idx;
    memcpy(&payload->msg, msg, sizeof(ClockSyncMessage));
    tag_t now = {.time = self->env->get_physical_time(self->env), .microstep = 0};
    SystemEvent event = SYSTEM_EVENT_INIT(now, &self->super, (void *)payload);
    ret = self->env->scheduler->schedule_at_locked(self->env->scheduler, &event.super);
    if (ret != LF_OK) {
      LF_ERR(CLOCK_SYNC, "Failed to schedule clock-sync system event.");
      assert(false);
    }
  } else {
    LF_WARN(CLOCK_SYNC, "Failed to allocate payload for clock-sync system event.");
  }
}

static void ClockSynchronization_handle_priority_request(ClockSynchronization *self, int src_neighbor) {
  int my_priority = ClockSynchronization_my_priority(self);
  lf_ret_t ret;

  if (src_neighbor == -1) {
    LF_DEBUG(CLOCK_SYNC, "Broadcasting priority %d to all neighbors", my_priority);
    for (size_t i = 0; i < self->num_neighbours; i++) {
      if (((int)i) == self->master_neighbor_index) {
        continue;
      }
      FederatedConnectionBundle *bundle = self->env->net_bundles[i];
      NetworkChannel *chan = bundle->net_channel;
      bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
      bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_priority_tag;
      bundle->send_msg.message.clock_sync_msg.message.priority.priority = my_priority;
      ret = chan->send_blocking(chan, &bundle->send_msg);
      if (ret != LF_OK) {
        LF_WARN(CLOCK_SYNC, "Failed to send priority to neighbor %zu", i);
      }
    }
  } else {
    LF_DEBUG(CLOCK_SYNC, "Handling priority request from neighbor %d replying with %d", src_neighbor, my_priority);
    FederatedConnectionBundle *bundle = self->env->net_bundles[src_neighbor];
    NetworkChannel *chan = bundle->net_channel;
    bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
    bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_priority_tag;
    bundle->send_msg.message.clock_sync_msg.message.priority.priority = my_priority;
    ret = chan->send_blocking(chan, &bundle->send_msg);
    validate(ret == LF_OK);
  }
}

static void ClockSynchronization_handle_priority_update(ClockSynchronization *self, int src_neighbor, int priority) {
  LF_DEBUG(CLOCK_SYNC, "Received priority %d from neighbor %d", priority, src_neighbor);
  int my_old_priority = ClockSynchronization_my_priority(self);
  self->neighbor_clock[src_neighbor].priority = priority;
  int my_new_priority = ClockSynchronization_my_priority(self);
  if (my_new_priority < my_old_priority) {
    LF_DEBUG(CLOCK_SYNC, "Received priority %d+1=%d less than current priority %d, updating", priority, my_new_priority,
             my_old_priority);
    self->master_neighbor_index = src_neighbor;
    ClockSynchronization_handle_priority_request(self, -1);
  }
}

static void ClockSynchronization_handle_delay_request(ClockSynchronization *self, SystemEvent *event) {
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  lf_ret_t ret;
  int src_neighbor = payload->neighbor_index;
  LF_DEBUG(CLOCK_SYNC, "Handling delay request from neighbor %d", src_neighbor);
  validate(src_neighbor >= 0);
  validate(src_neighbor < (int)self->num_neighbours);
  FederatedConnectionBundle *bundle = self->env->net_bundles[src_neighbor];
  NetworkChannel *chan = bundle->net_channel;
  bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
  bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_delay_response_tag;
  bundle->send_msg.message.clock_sync_msg.message.sync_response.time = event->super.tag.time;
  bundle->send_msg.message.clock_sync_msg.message.sync_response.sequence_number =
      payload->msg.message.delay_request.sequence_number;
  ret = chan->send_blocking(chan, &bundle->send_msg);
  if (ret != LF_OK) {
    LF_WARN(CLOCK_SYNC, "Failed to send DelayResponse to neighbor %zu", src_neighbor);
  }
}

static void ClockSynchronization_handle_sync_response(ClockSynchronization *self, SystemEvent *event) {
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  lf_ret_t ret;
  SyncResponse *msg = &payload->msg.message.sync_response;
  int src_neighbor = payload->neighbor_index;
  LF_DEBUG(CLOCK_SYNC, "Handling sync response from neighbor %d", src_neighbor);

  if (msg->sequence_number != self->sequence_number) {
    LF_WARN(CLOCK_SYNC, "Received out-of-order sync response %d from neighbor %d dropping", msg->sequence_number,
            src_neighbor);
    return;
  }

  self->timestamps.t1 = msg->time;
  self->timestamps.t2 = event->super.tag.time;

  FederatedConnectionBundle *bundle = self->env->net_bundles[src_neighbor];
  NetworkChannel *chan = bundle->net_channel;
  bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
  bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_delay_request_tag;
  bundle->send_msg.message.clock_sync_msg.message.delay_request.sequence_number = ++self->sequence_number;
  self->timestamps.t3 = self->env->get_physical_time(self->env);
  ret = chan->send_blocking(chan, &bundle->send_msg);
  if (ret != LF_OK) {
    LF_WARN(CLOCK_SYNC, "Failed to send DelayRequest to neighbor %zu. Updating priorities", src_neighbor);
    ClockSynchronization_handle_priority_update(self, src_neighbor, UNKNOWN_PRIORITY);
  }
}

static void ClockSynchronization_handle_delay_response(ClockSynchronization *self, SystemEvent *event) {
  DelayResponse *msg = &((ClockSyncEvent *)event->super.payload)->msg.message.delay_response;
  LF_DEBUG(CLOCK_SYNC, "Handling delay response");
  if (msg->sequence_number != self->sequence_number) {
    LF_WARN(CLOCK_SYNC, "Received out-of-order sync response %d, dropping", msg->sequence_number);
    return;
  }
  self->timestamps.t4 = msg->time;
  ClockSynchronization_correct_clock(self, &self->timestamps);
}

static void ClockSynchronization_handle_request_sync(ClockSynchronization *self, SystemEvent *event) {
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  lf_ret_t ret;
  int src_neighbor = payload->neighbor_index;
  if (src_neighbor == -1) {
    if (self->master_neighbor_index >= 0) {
      LF_DEBUG(CLOCK_SYNC, "Sending out ReguestSync to master neighbor %d", self->master_neighbor_index);
      FederatedConnectionBundle *bundle = self->env->net_bundles[self->master_neighbor_index];
      NetworkChannel *chan = bundle->net_channel;
      bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
      bundle->send_msg.message.clock_sync_msg.message.request_sync.sequence_number = ++self->sequence_number;
      bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_request_sync_tag;
      ret = chan->send_blocking(chan, &bundle->send_msg);
      if (ret != LF_OK) {
        LF_WARN(CLOCK_SYNC, "Failed to send RequestSync to master neighbor %d. Resetting priority and master neighbor",
                self->master_neighbor_index);
        ClockSynchronization_handle_priority_update(self, src_neighbor, UNKNOWN_PRIORITY);
      }

    } else {
      LF_DEBUG(CLOCK_SYNC, "No master neighbor, wait for next sync round.");
    }

    LF_DEBUG(CLOCK_SYNC, "Scheduling next RequestSync");
    ClockSynchronization_schedule_system_event(self, self->env->get_physical_time(self->env) + CLOCK_SYNC_PERIOD,
                                               ClockSyncMessage_request_sync_tag);
  } else {
    LF_DEBUG(CLOCK_SYNC, "Handling RequestSync from neighbor %d", src_neighbor);
    FederatedConnectionBundle *bundle = self->env->net_bundles[src_neighbor];
    NetworkChannel *chan = bundle->net_channel;
    bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
    bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_sync_response_tag;
    bundle->send_msg.message.clock_sync_msg.message.sync_response.time = self->env->get_physical_time(self->env);
    bundle->send_msg.message.clock_sync_msg.message.sync_response.sequence_number =
        payload->msg.message.request_sync.sequence_number;
    ret = chan->send_blocking(chan, &bundle->send_msg);
    if (ret != LF_OK) {
      LF_WARN(CLOCK_SYNC, "Failed to send SyncResponse to neighbor %d", src_neighbor);
    }
  }
}

static void ClockSynchronization_handle_system_event(SystemEventHandler *_self, SystemEvent *event) {
  ClockSynchronization *self = (ClockSynchronization *)_self;
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  LF_DEBUG(CLOCK_SYNC, "Handle ClockSync system event %d from neighbor %d", payload->msg.which_message,
           payload->neighbor_index);

  switch (payload->msg.which_message) {
  case ClockSyncMessage_priority_request_tag:
    ClockSynchronization_handle_priority_request(self, payload->neighbor_index);
    break;
  case ClockSyncMessage_priority_tag:
    ClockSynchronization_handle_priority_update(self, payload->neighbor_index, payload->msg.message.priority.priority);
    break;
  case ClockSyncMessage_request_sync_tag:
    ClockSynchronization_handle_request_sync(self, event);
    break;
  case ClockSyncMessage_sync_response_tag:
    ClockSynchronization_handle_sync_response(self, event);
    break;
  case ClockSyncMessage_delay_request_tag:
    ClockSynchronization_handle_delay_request(self, event);
    break;
  case ClockSyncMessage_delay_response_tag:
    ClockSynchronization_handle_delay_response(self, event);
    break;
  }

  self->super.payload_pool.free(&self->super.payload_pool, event->super.payload);
}

void ClockSynchronization_ctor(ClockSynchronization *self, Environment *env, NeighborClock *neighbor_clock,
                               size_t num_neighbors, bool is_grandmaster, size_t payload_size, void *payload_buf,
                               bool *payload_used_buf, size_t payload_buf_capacity) {
  self->env = env;
  self->neighbor_clock = neighbor_clock;
  self->num_neighbours = num_neighbors;
  self->is_grandmaster = is_grandmaster;
  self->master_neighbor_index = -1;
  self->sequence_number = -1;
  self->handle_message_callback = ClockSynchronization_handle_message_callback;
  self->super.handle = ClockSynchronization_handle_system_event;
  self->clock_sync_timer_event.neighbor_index = -1;
  self->clock_sync_timer_event.msg.which_message = ClockSyncMessage_request_sync_tag;

  EventPayloadPool_ctor(&self->super.payload_pool, payload_buf, payload_used_buf, payload_size, payload_buf_capacity);

  for (size_t i = 0; i < num_neighbors; i++) {
    self->neighbor_clock[i].priority = UNKNOWN_PRIORITY;
  }

  // Initialize the servo.
  self->servo.Kp = SERVO_KP;
  self->servo.Ki = SERVO_KI;
  self->servo.accumulated_error = 0;
  self->servo.last_error = 0;
  self->servo.clamp = SERVO_CLAMP_PPB;

  // If we are a grandmaster, schedule the broadcast of the priority.
  if (self->is_grandmaster) {
    ClockSynchronization_schedule_system_event(self, 0, ClockSyncMessage_priority_request_tag);
  } else {
    // If we are not a grandmaster, schedule the periodic request for sync.
    ClockSynchronization_schedule_system_event(self, 0, ClockSyncMessage_request_sync_tag);
  }
}