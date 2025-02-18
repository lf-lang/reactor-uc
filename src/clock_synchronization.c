#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/error.h"
#include "reactor-uc/logging.h"

#define NO_PRIORITY INT32_MAX
#define CLOCK_SYNC_PERIOD SEC(1)

static void ClockSynchronization_correct_clock(ClockSynchronization *self, ClockSyncTimestamps *timestamps) {
  interval_t rtt = (timestamps->t4 - timestamps->t1) - (timestamps->t3 - timestamps->t2);
  interval_t clock_offset = rtt/2 - (timestamps->t2 - timestamps->t1);

  self->servo.last_error = clock_offset;
  self->servo.accumulated_error += clock_offset;
  
  float correction_float = self->servo.Kp * clock_offset + self->servo.Ki * self->servo.accumulated_error;
  interval_t correction = (interval_t)correction_float;
  if (correction > self->servo.clamp) {
    correction = self->servo.clamp;
  } else if (correction < -self->servo.clamp) {
    correction = -self->servo.clamp;
  }

  self->env->clock.adjust_time(self->env, correction);
}

static int ClockSynchronization_my_priority(ClockSynchronization *self) {
  if (self->is_grandmaster) {
    return 0;
  }

  int my_priority = NO_PRIORITY;
  for (size_t i = 0; i < self->num_neighbours; i++) {
    int neighbor_priority = self->neighbor_clock[i].priority;
    if (neighbor_priority != NO_PRIORITY && (neighbor_priority + 1) < my_priority) {
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
  LF_DEBUG(SCHED, "Received clock sync message from neighbor %zu. Scheduling as a system event", bundle_idx);
  self->env->enter_critical_section(self->env);
  ClockSyncEvent *payload = NULL;
  lf_ret_t ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void **)&payload);
  if (ret == LF_OK) {
    payload->neighbor_index = bundle_idx;
    memcpy(&payload->msg, msg, sizeof(ClockSyncMessage));
    tag_t now = {.time = self->env->get_physical_time(self->env), .microstep = 0};
    SystemEvent event = SYSTEM_EVENT_INIT(now, &self->super, (void *)payload);
    ret = self->env->scheduler->schedule_at_locked(self->env->scheduler, &event.super);
    if (ret != LF_OK) {
      LF_ERR(SCHED, "Failed to schedule clock-sync system event.");
    }
  } else {
    LF_ERR(SCHED, "Failed to allocate payload for clock-sync system event.");
  }
  self->env->leave_critical_section(self->env);
}
static void ClockSynchronization_handle_priority(ClockSynchronization *self, int src_neighbor,
                                                            int priority) {
  int my_old_priority = ClockSynchronization_my_priority(self);
  self->neighbor_clock[src_neighbor].priority = priority;
  int my_new_priority = ClockSynchronization_my_priority(self);
  if (my_new_priority < my_old_priority) {
    self->master_neighbor_index = src_neighbor;
    ClockSynchronization_handle_priority_request(self, -1);
  }
}

static void ClockSynchronization_handle_priority_request(ClockSynchronization *self, int src_neighbor) {
  int my_priority = ClockSynchronization_my_priority(self);

  if (src_neighbor == -1) {
    for (int i = 0; i < self->num_neighbours; i++) {
      if (i == self->master_neighbor_index) {
        continue;
      }
      FederatedConnectionBundle *bundle = self->env->net_bundles[i];
      NetworkChannel *chan = bundle->net_channel;
      bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
      bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_priority_tag;
      bundle->send_msg.message.clock_sync_msg.message.priority.priority = my_priority;
      chan->send_blocking(chan, &bundle->send_msg);
    }
  } else {
    FederatedConnectionBundle *bundle = self->env->net_bundles[src_neighbor];
    NetworkChannel *chan = bundle->net_channel;
    bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
    bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_priority_tag;
    bundle->send_msg.message.clock_sync_msg.message.priority.priority = my_priority;
    chan->send_blocking(chan, &bundle->send_msg);
  }
}

static void ClockSynchronization_handle_delay_request(ClockSynchronization *self, SystemEvent *event) {
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  int src_neighbor = payload->neighbor_index;
  validate(src_neighbor >= 0);
  validate(src_neighbor < self->num_neighbours);
  FederatedConnectionBundle *bundle = self->env->net_bundles[src_neighbor];
  NetworkChannel *chan = bundle->net_channel;
  bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
  bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_delay_response_tag;
  bundle->send_msg.message.clock_sync_msg.message.sync_response.time = event->super.tag.time;
  bundle->send_msg.message.clock_sync_msg.message.sync_response.sequence_number =
      payload->msg.message.delay_request.sequence_number;
  chan->send_blocking(chan, &bundle->send_msg);
}

static void ClockSynchronization_handle_sync_response(ClockSynchronization *self, SystemEvent *event) {
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  SyncResponse *msg = &payload->msg.message.sync_response;
  int src_neighbor = payload->neighbor_index;

  if (msg->sequence_number != self->sequence_number) {
    LF_WARN(SCHED, "Received out-of-order sync response %d from neighbor %d dropping", msg->sequence_number,
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
  chan->send_blocking(chan, &bundle->send_msg);
}

static void ClockSynchronization_handle_delay_response(ClockSynchronization *self, SystemEvent *event) {

}

static void ClockSynchronization_handle_request_sync(ClockSynchronization *self, SystemEvent *event) {
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  int src_neighbor = payload->neighbor_index;
  if (src_neighbor == -1) {
    if (self->master_neighbor_index >= 0) {
      FederatedConnectionBundle *bundle = self->env->net_bundles[self->master_neighbor_index];
      NetworkChannel *chan = bundle->net_channel;
      bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
      bundle->send_msg.message.clock_sync_msg.message.request_sync.sequence_number = ++self->sequence_number;
      bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_request_sync_tag;
      chan->send_blocking(chan, &bundle->send_msg);
    }

    ClockSynchronization_schedule_system_event(self, self->env->get_physical_time(self->env) + CLOCK_SYNC_PERIOD,
                                               ClockSyncMessage_request_sync_tag);
  } else {
    FederatedConnectionBundle *bundle = self->env->net_bundles[src_neighbor];
    NetworkChannel *chan = bundle->net_channel;
    bundle->send_msg.which_message = FederateMessage_clock_sync_msg_tag;
    bundle->send_msg.message.clock_sync_msg.which_message = ClockSyncMessage_sync_response_tag;
    bundle->send_msg.message.clock_sync_msg.message.sync_response.time = self->env->get_physical_time(self->env);
    bundle->send_msg.message.clock_sync_msg.message.sync_response.sequence_number =
        payload->msg.message.request_sync.sequence_number;
    chan->send_blocking(chan, &bundle->send_msg);
  }
}

static void ClockSynchronization_handle_system_event(SystemEventHandler *_self, SystemEvent *event) {
  ClockSynchronization *self = (ClockSynchronization *)_self;
  ClockSyncEvent *payload = (ClockSyncEvent *)event->super.payload;
  LF_DEBUG(SCHED, "Handle ClockSync system event from neighbor %zu", payload->neighbor_index);

  switch (payload->msg.which_message) {
  case ClockSyncMessage_priority_request_tag:
    ClockSynchronization_handle_priority_request(self, payload->neighbor_index);
    break;
  case ClockSyncMessage_priority_tag:
    ClockSynchronization_handle_priority(self, payload->neighbor_index,
                                                    payload->msg.message.priority.priority);
    break;
  case ClockSyncMessage_request_sync_tag:
    break;
  case ClockSyncMessage_sync_response_tag:
    break;
  case ClockSyncMessage_delay_request_tag:
    break;
  case ClockSyncMessage_delay_response_tag:
    break;
  }

  self->super.payload_pool.free(&self->super.payload_pool, event->super.payload);
}

static void ClockSynchronization_schedule_system_event(ClockSynchronization *self, instant_t time, int message_type) {
  ClockSyncEvent *payload = NULL;
  lf_ret_t ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void **)&payload);
  if (ret != LF_OK) {
    LF_ERR(SCHED, "Failed to allocate payload for clock-sync system event.");
    return;
  }
  payload->neighbor_index = -1; // -1 means that it is from ourself
  ClockSyncMessage msg = {.which_message = message_type};
  tag_t tag = {.time = time, .microstep = 0};
  SystemEvent event = SYSTEM_EVENT_INIT(tag, &self->super, (void *)payload);
  // TODO: Critical section?

  ret = self->env->scheduler->schedule_at_locked(self->env->scheduler, &event.super);
  if (ret != LF_OK) {
    LF_ERR(SCHED, "Failed to schedule clock-sync system event.");
  }
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
  EventPayloadPool_ctor(&self->super.payload_pool, payload_buf, payload_used_buf, payload_size, payload_buf_capacity);

  for (size_t i = 0; i < num_neighbors; i++) {
    self->neighbor_clock[i].priority = NO_PRIORITY;
  }

  // If we are a grandmaster, schedule the broadcast of the priority.
  if (self->is_grandmaster) {
    ClockSynchronization_schedule_system_event(self, NEVER, ClockSyncMessage_clock_sync_priority_request_tag);
  } else {
    // If we are not a grandmaster, schedule the periodic request for sync.
    ClockSynchronization_schedule_system_event(self, 0, ClockSyncMessage_request_sync_tag);
  }
}