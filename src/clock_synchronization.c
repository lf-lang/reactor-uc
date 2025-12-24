#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/error.h"
#include "reactor-uc/logging.h"
#include "proto/message.pb.h"

#define UNKNOWN_PRIORITY INT32_MAX // If priority is unknown we set it to the maximum int32 value
#define GRANDMASTER_PRIORITY 0     // The grandmaster has priority of 0
#define NEIGHBOR_INDEX_SELF -1
#define NEIGHBOR_INDEX_UNKNOWN -2
#define NUM_RESERVED_EVENTS 2 // There is 1 periodic event, but it is rescheduled before it is freed so we need 2.

static void ClockSynchronization_correct_clock(ClockSynchronization* self, ClockSyncTimestamps* timestamps) {
  LF_DEBUG(CLOCK_SYNC, "Correcting clock. T1=" PRINTF_TIME " T2=" PRINTF_TIME " T3=" PRINTF_TIME " T4=" PRINTF_TIME,
           timestamps->t1, timestamps->t2, timestamps->t3, timestamps->t4);
  interval_t rtt = (timestamps->t4 - timestamps->t1) - (timestamps->t3 - timestamps->t2);
  interval_t owd = rtt / 2;
  interval_t clock_offset = owd - (timestamps->t2 - timestamps->t1);
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  LF_DEBUG(CLOCK_SYNC, "RTT: " PRINTF_TIME " OWD: " PRINTF_TIME " offset: " PRINTF_TIME, rtt, owd, clock_offset);

  // The very first iteration of clock sync we possibly step the clock (forwards or backwards)
  if (!self->has_initial_sync) {
    interval_t clock_offset_abs = clock_offset > 0 ? clock_offset : -clock_offset;
    self->has_initial_sync = true;
    if (clock_offset_abs > CLOCK_SYNC_INITAL_STEP_THRESHOLD) {
      LF_INFO(CLOCK_SYNC, "Initial clock offset to grand master is " PRINTF_TIME
                          " which is greater than the initial step threshold. Stepping clock");

      env_fed->clock.set_time(&env_fed->clock, env_fed->clock.get_time(&env_fed->clock) + clock_offset);
      // Also inform the scheduler that we have stepped the clock so it can adjust timestamps
      // of pending events.
      self->env->scheduler->step_clock(self->env->scheduler, clock_offset);
      self->env->platform->notify(self->env->platform);
      return;
    }
  }

  // Record last error. Currently unused, but can be used to implement the derivative part of a PID
  self->servo.last_error = clock_offset;
  // Integrate the error, used for the integral part of the PID.
  self->servo.accumulated_error += clock_offset;
  // Compute correction as a PID controller.
  float correction_float = self->servo.Kp * clock_offset + self->servo.Ki * self->servo.accumulated_error;
  // Convert to integer.
  interval_t correction = (interval_t)correction_float;

  // Clamp the correction
  if (correction > self->servo.clamp) {
    correction = self->servo.clamp;
  } else if (correction < -self->servo.clamp) {
    correction = -self->servo.clamp;
  }

  LF_DEBUG(CLOCK_SYNC, "Accumulated error: " PRINTF_TIME " Correction: " PRINTF_TIME, self->servo.accumulated_error,
           correction);

  env_fed->clock.adjust_time(&env_fed->clock, correction);
}

/** Send our current clock priority to all connected neighbors. */
static void ClockSynchronization_broadcast_priority(ClockSynchronization* self) {
  lf_ret_t ret;
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  LF_DEBUG(CLOCK_SYNC, "Broadcasting priority %d to all neighbors", self->my_priority);
  for (size_t i = 0; i < self->num_neighbours; i++) {
    // Do not send out the priority to the master neighbor, because this is the origin
    // of our priority.
    if (((int)i) == self->master_neighbor_index) {
      continue;
    }

    FederatedConnectionBundle* bundle = env_fed->net_bundles[i];
    NetworkChannel* chan = bundle->net_channel;
    bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
    bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_priority_tag;
    bundle->send_msg.message.clock_sync_msg.message.priority.priority = self->my_priority;
    ret = chan->send_blocking(chan, &bundle->send_msg);
    if (ret != LF_OK) {
      LF_WARN(CLOCK_SYNC, "Failed to send priority to neighbor %zu", i);
    }
  }
}

/**
 * @brief
 *  Schedule a system event for the given time with the given message type with source neighbor set to self.
 */
static void ClockSynchronization_schedule_system_event(ClockSynchronization* self, instant_t time, int message_type) {
  ClockSyncEvent* payload = NULL;
  lf_ret_t ret;

  ret = self->super.payload_pool.allocate_reserved(&self->super.payload_pool, (void**)&payload);

  if (ret != LF_OK) {
    LF_ERR(CLOCK_SYNC, "Failed to allocate payload for clock-sync system event.");
    validate(false);
    return;
  }
  payload->neighbor_index = NEIGHBOR_INDEX_SELF; // Means that we are the source of the message.
  payload->msg.which_message = message_type;
  tag_t tag = {.time = time, .microstep = 0};
  SystemEvent event = SYSTEM_EVENT_INIT(tag, &self->super, payload);

  ret = self->env->scheduler->schedule_system_event_at(self->env->scheduler, &event);
  if (ret != LF_OK) {
    LF_ERR(CLOCK_SYNC, "Failed to schedule clock-sync system event.");
    self->super.payload_pool.free(&self->super.payload_pool, payload);
    validate(false);
  }
}

/** Compute my clock sync priority and also set the correct index of the master neighbor. */
static void ClockSynchronization_update_neighbor_priority(ClockSynchronization* self, int neighbor, int priority) {
  self->neighbor_clock[neighbor].priority = priority;
  if (self->is_grandmaster) {
    self->master_neighbor_index = -1;
    self->my_priority = GRANDMASTER_PRIORITY;
  } else {
    int my_priority = UNKNOWN_PRIORITY;
    int gm_index = NEIGHBOR_INDEX_UNKNOWN;
    for (size_t i = 0; i < self->num_neighbours; i++) {
      int neighbor_priority = self->neighbor_clock[i].priority;
      if (neighbor_priority != UNKNOWN_PRIORITY && (neighbor_priority + 1) < my_priority) {
        my_priority = neighbor_priority + 1;
        gm_index = i;
      }
    }
    self->master_neighbor_index = gm_index;
    self->my_priority = my_priority;
  }
}

/** Handle incoming network messages with ClockSyncMessages. Called from async context. */
static void ClockSynchronization_handle_message_callback(ClockSynchronization* self, const ClockSyncMessage* msg,
                                                         size_t bundle_idx) {
  LF_DEBUG(CLOCK_SYNC, "Received clock sync message from neighbor %zu. Scheduling as a system event", bundle_idx);
  ClockSyncEvent* payload = NULL;
  tag_t tag = {.time = self->env->get_physical_time(self->env), .microstep = 0};

  lf_ret_t ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void**)&payload);

  if (ret == LF_OK) {
    payload->neighbor_index = bundle_idx;
    memcpy(&payload->msg, msg, sizeof(ClockSyncMessage));

    SystemEvent event = SYSTEM_EVENT_INIT(tag, &self->super, payload);
    ret = self->env->scheduler->schedule_system_event_at(self->env->scheduler, &event);
    if (ret != LF_OK) {
      LF_ERR(CLOCK_SYNC, "Failed to schedule clock-sync system event.");
      validate(false); // This should not be possible.
    }
  } else {
    LF_WARN(CLOCK_SYNC, "Failed to allocate payload for clock-sync system event.");
  }
}

static void ClockSynchronization_handle_priority_request(ClockSynchronization* self, int src_neighbor) {
  lf_ret_t ret;
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  if (src_neighbor == NEIGHBOR_INDEX_SELF) {
    LF_DEBUG(CLOCK_SYNC, "Send clock sync priority requests to all neighbors");
    for (size_t i = 0; i < self->num_neighbours; i++) {
      // Do not send out the priority to the master neighbor, because this is the origin
      // of our priority.
      if (((int)i) == self->master_neighbor_index) {
        continue;
      }

      FederatedConnectionBundle* bundle = env_fed->net_bundles[i];
      NetworkChannel* chan = bundle->net_channel;
      bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
      bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_priority_request_tag;
      ret = chan->send_blocking(chan, &bundle->send_msg);
      if (ret != LF_OK) {
        LF_WARN(CLOCK_SYNC, "Failed to request priority to neighbor %zu", i);
      }
    }
  } else {
    LF_DEBUG(CLOCK_SYNC, "Handling priority request from neighbor %d replying with %d", src_neighbor,
             self->my_priority);
    FederatedConnectionBundle* bundle = env_fed->net_bundles[src_neighbor];
    NetworkChannel* chan = bundle->net_channel;
    bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
    bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_priority_tag;
    bundle->send_msg.message.clock_sync_msg.message.priority.priority = self->my_priority;
    ret = chan->send_blocking(chan, &bundle->send_msg);
    if (ret != LF_OK) {
      LF_WARN(CLOCK_SYNC, "Failed to send priority to neighbor %zu", src_neighbor);
    }
  }
}

/** Update the priority of a neighbour and possibly update our own priority and broadcast it, if it has changed. */
static void ClockSynchronization_handle_priority_update(ClockSynchronization* self, int src_neighbor, int priority) {
  LF_DEBUG(CLOCK_SYNC, "Received priority %d from neighbor %d", priority, src_neighbor);
  int my_old_priority = self->my_priority;
  ClockSynchronization_update_neighbor_priority(self, src_neighbor, priority);
  int my_new_priority = self->my_priority;
  if (my_new_priority != my_old_priority) {
    LF_DEBUG(CLOCK_SYNC, "Received priority changes my priority from %d to %d, broadcasting", my_old_priority,
             my_new_priority);
    ClockSynchronization_broadcast_priority(self);
  }
}

/** Handle a DelayRequest message from a slave. Respond with the time at which the request was received. */
static void ClockSynchronization_handle_delay_request(ClockSynchronization* self, SystemEvent* event) {
  lf_ret_t ret;
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  ClockSyncEvent* payload = (ClockSyncEvent*)event->super.payload;
  int src_neighbor = payload->neighbor_index;
  LF_DEBUG(CLOCK_SYNC, "Handling delay request from neighbor %d", src_neighbor);
  assert(src_neighbor >= 0);
  assert(src_neighbor < (int)self->num_neighbours);
  FederatedConnectionBundle* bundle = env_fed->net_bundles[src_neighbor];
  NetworkChannel* chan = bundle->net_channel;
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

/** Handle a SyncResponse from a master. Record the time of arrival and send a DelayRequest. */
static void ClockSynchronization_handle_sync_response(ClockSynchronization* self, SystemEvent* event) {
  ClockSyncEvent* payload = (ClockSyncEvent*)event->super.payload;
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  lf_ret_t ret;
  SyncResponse* msg = &payload->msg.message.sync_response;
  int src_neighbor = payload->neighbor_index;
  LF_DEBUG(CLOCK_SYNC, "Handling sync response from neighbor %d", src_neighbor);

  if (msg->sequence_number != self->sequence_number) {
    LF_WARN(CLOCK_SYNC, "Received out-of-order sync response %d from neighbor %d dropping", msg->sequence_number,
            src_neighbor);
    return;
  }

  self->timestamps.t1 = msg->time;
  self->timestamps.t2 = event->super.tag.time;

  FederatedConnectionBundle* bundle = env_fed->net_bundles[src_neighbor];
  NetworkChannel* chan = bundle->net_channel;
  bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
  bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_delay_request_tag;
  bundle->send_msg.message.clock_sync_msg.message.delay_request.sequence_number = self->sequence_number;
  self->timestamps.t3 = self->env->get_physical_time(self->env);
  ret = chan->send_blocking(chan, &bundle->send_msg);
  if (ret != LF_OK) {
    LF_WARN(CLOCK_SYNC, "Failed to send DelayRequest to neighbor %zu. Updating priorities", src_neighbor);
    ClockSynchronization_handle_priority_update(self, src_neighbor, UNKNOWN_PRIORITY);
  }
}

/** Handle a DelayResponse from a master. This completes a sync round and we can run the PI controller. */
static void ClockSynchronization_handle_delay_response(ClockSynchronization* self, SystemEvent* event) {
  DelayResponse* msg = &((ClockSyncEvent*)event->super.payload)->msg.message.delay_response;
  LF_DEBUG(CLOCK_SYNC, "Handling delay response");
  if (msg->sequence_number != self->sequence_number) {
    LF_WARN(CLOCK_SYNC, "Received out-of-order sync response %d, dropping", msg->sequence_number);
    return;
  }
  self->timestamps.t4 = msg->time;
  ClockSynchronization_correct_clock(self, &self->timestamps);
}

/** Handle a SyncRequest message from a slave. Repond with SyncResponse which contains the time of its transmission. */
static void ClockSynchronization_handle_request_sync(ClockSynchronization* self, SystemEvent* event) {
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  ClockSyncEvent* payload = (ClockSyncEvent*)event->super.payload;
  lf_ret_t ret;
  int src_neighbor = payload->neighbor_index;
  if (src_neighbor == NEIGHBOR_INDEX_SELF) {
    if (self->master_neighbor_index >= 0) {
      LF_DEBUG(CLOCK_SYNC, "Sending out ReguestSync to master neighbor %d", self->master_neighbor_index);
      FederatedConnectionBundle* bundle = env_fed->net_bundles[self->master_neighbor_index];
      NetworkChannel* chan = bundle->net_channel;
      bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
      bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_request_sync_tag;
      bundle->send_msg.message.clock_sync_msg.message.request_sync.sequence_number = ++self->sequence_number;
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
    ClockSynchronization_schedule_system_event(self, self->env->get_physical_time(self->env) + self->period,
                                               ClockSyncMessage_request_sync_tag);
  } else {
    LF_DEBUG(CLOCK_SYNC, "Handling RequestSync from neighbor %d", src_neighbor);
    FederatedConnectionBundle* bundle = env_fed->net_bundles[src_neighbor];
    NetworkChannel* chan = bundle->net_channel;
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

/** Handle system events. This is done synchronously from the runtime context. */
static void ClockSynchronization_handle_system_event(SystemEventHandler* _self, SystemEvent* event) {
  ClockSynchronization* self = (ClockSynchronization*)_self;
  ClockSyncEvent* payload = (ClockSyncEvent*)event->super.payload;
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
  case ClockSyncMessage_priority_broadcast_request_tag:
    ClockSynchronization_broadcast_priority(self);
    break;
  }

  self->super.payload_pool.free(&self->super.payload_pool, event->super.payload);
}

void ClockSynchronization_ctor(ClockSynchronization* self, Environment* env, NeighborClock* neighbor_clock,
                               size_t num_neighbors, bool is_grandmaster, size_t payload_size, void* payload_buf,
                               bool* payload_used_buf, size_t payload_buf_capacity, interval_t period,
                               interval_t max_adj, float servo_kp, float servo_ki) {
  self->env = env;
  self->neighbor_clock = neighbor_clock;
  self->num_neighbours = num_neighbors;
  self->is_grandmaster = is_grandmaster;
  self->master_neighbor_index = -1;
  self->sequence_number = -1;
  self->handle_message_callback = ClockSynchronization_handle_message_callback;
  self->super.handle = ClockSynchronization_handle_system_event;
  self->period = period;

  EventPayloadPool_ctor(&self->super.payload_pool, (char*)payload_buf, payload_used_buf, payload_size,
                        payload_buf_capacity, NUM_RESERVED_EVENTS);

  for (size_t i = 0; i < num_neighbors; i++) {
    self->neighbor_clock[i].priority = UNKNOWN_PRIORITY;
  }

  // Initialize the servo.
  self->servo.Kp = servo_kp;
  self->servo.Ki = servo_ki;
  self->servo.accumulated_error = 0;
  self->servo.last_error = 0;
  self->servo.clamp = max_adj;

  // If we are a grandmaster, schedule the broadcast of the priority.
  if (self->is_grandmaster) {
    ClockSynchronization_schedule_system_event(self, 0, ClockSyncMessage_priority_broadcast_request_tag);
  } else {
    // If we are not a grandmaster, schedule the periodic request for sync.
    ClockSynchronization_schedule_system_event(self, 0, ClockSyncMessage_request_sync_tag);
  }
}