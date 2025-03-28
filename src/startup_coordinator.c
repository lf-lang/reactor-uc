#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/environments/environment_federated.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/logging.h"
#include "proto/message.pb.h"

#define NEIGHBOR_INDEX_SELF -1
#define NUM_RESERVED_EVENTS 3 // 3 events is reserved for scheduling our own events.

/**
 * @brief Open connections to all neighbors. This function will block until all connections are established.
 */
static lf_ret_t StartupCoordinator_connect_to_neighbors_blocking(StartupCoordinator *self) {
  EnvironmentFederated *env_fed = (EnvironmentFederated *)self->env;
  validate(self->state == StartupCoordinationState_UNINITIALIZED);
  self->state = StartupCoordinationState_CONNECTING;
  LF_INFO(FED, "%s connecting to %zu federated peers", self->env->main->name, env_fed->net_bundles_size);
  lf_ret_t ret;

  // Open all connections.
  for (size_t i = 0; i < env_fed->net_bundles_size; i++) {
    FederatedConnectionBundle *bundle = env_fed->net_bundles[i];
    NetworkChannel *chan = bundle->net_channel;
    ret = chan->open_connection(chan);
    // If we cannot open our network channels, then we cannot proceed and must abort.
    validate(ret == LF_OK);
  }

  // Do a busy-loop until all connections are established.
  bool all_connected = false;
  interval_t wait_before_retry = NEVER;
  while (!all_connected) {
    // Wait time initialized to minimum value so we can find the maximum.
    all_connected = true;
    for (size_t i = 0; i < self->num_neighbours; i++) {
      NetworkChannel *chan = env_fed->net_bundles[i]->net_channel;
      // Check whether the neighbor has reached the desired state.
      if (!chan->is_connected(chan)) {
        // If a retry function is provided, call it.
        all_connected = false;
        // Check if the expected_connect_duration is longer than the current wait time.
        if (chan->expected_connect_duration > wait_before_retry) {
          wait_before_retry = chan->expected_connect_duration;
        }
      }
    }
    if (!all_connected) {
      // This will release the critical section and allow other tasks to run.
      self->env->wait_until_locked(self->env, self->env->get_physical_time(self->env) + wait_before_retry);
    }
  }

  LF_INFO(FED, "%s Established connection to all %zu federated peers", self->env->main->name,
          env_fed->net_bundles_size);
  self->state = StartupCoordinationState_HANDSHAKING;
  return LF_OK;
}

/** Schedule a system-event with `self` as the origin for some future time. */
static void StartupCoordinator_schedule_system_self_event(StartupCoordinator *self, instant_t time, int message_type) {
  StartupEvent *payload = NULL;
  lf_ret_t ret;
  // Allocate one of the reserved events for our own use.
  ret = self->super.payload_pool.allocate_reserved(&self->super.payload_pool, (void **)&payload);
  if (ret != LF_OK) {
    LF_ERR(FED, "Failed to allocate payload for startup system event.");
    // This is a critical error as we should have enough events reserved for our own use.
    validate(false);
    return;
  }
  payload->neighbor_index = NEIGHBOR_INDEX_SELF;
  payload->msg.which_message = message_type;
  tag_t tag = {.time = time, .microstep = 0};
  SystemEvent event = SYSTEM_EVENT_INIT(tag, &self->super, (void *)payload);

  ret = self->env->scheduler->schedule_at_locked(self->env->scheduler, &event.super);
  if (ret != LF_OK) {
    LF_ERR(FED, "Failed to schedule startup system event.");
    self->super.payload_pool.free(&self->super.payload_pool, payload);
    // This is a critical error as we should have place in the system event queue if we could allocate a payload.
    validate(false);
  }
}

/** Handle an incoming message from the network. Invoked from an async context in a critical section. */
static void StartupCoordinator_handle_message_callback(StartupCoordinator *self, const StartupCoordination *msg,
                                                       size_t bundle_idx) {
  LF_DEBUG(FED, "Received startup message from neighbor %zu. Scheduling as a system event", bundle_idx);
  ClockSyncEvent *payload = NULL;
  lf_ret_t ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void **)&payload);
  if (ret == LF_OK) {
    payload->neighbor_index = bundle_idx;
    memcpy(&payload->msg, msg, sizeof(StartupCoordination));
    tag_t now = {.time = self->env->get_physical_time(self->env), .microstep = 0};
    SystemEvent event = SYSTEM_EVENT_INIT(now, &self->super, (void *)payload);
    ret = self->env->scheduler->schedule_at_locked(self->env->scheduler, &event.super);
    if (ret != LF_OK) {
      LF_ERR(FED, "Failed to schedule startup system event.");
      self->super.payload_pool.free(&self->super.payload_pool, payload);
      // Critical error, there should be place in the system event queue if we could allocate a payload.
      validate(false);
    }
  } else {
    LF_ERR(FED, "Failed to allocate payload for incoming startup coordination message. Dropping it");
  }
}

/** Handle a request, either local or external, to do a startup handshake. This is called from the runtime context. */
static void StartupCoordinator_handle_startup_handshake_request(StartupCoordinator *self, StartupEvent *payload) {
  lf_ret_t ret;
  EnvironmentFederated *env_fed = (EnvironmentFederated *)self->env;
  if (payload->neighbor_index == NEIGHBOR_INDEX_SELF) {
    LF_DEBUG(FED, "Received handshake request from self");
    switch (self->state) {
    case StartupCoordinationState_HANDSHAKING:
      for (size_t i = 0; i < self->num_neighbours; i++) {
        FederateMessage *msg = &env_fed->net_bundles[i]->send_msg;
        NetworkChannel *chan = env_fed->net_bundles[i]->net_channel;

        if (!self->neighbor_state[i].handshake_response_received) {
          msg->which_message = FederateMessage_startup_coordination_tag;
          msg->message.startup_coordination.which_message = StartupCoordination_startup_handshake_request_tag;
          do {
            ret = chan->send_blocking(chan, msg);
          } while (ret != LF_OK);
        }
      }
      break;
    default:
      validate(false);
    }

  } else {
    // We can receive a handshake request at any point.
    LF_DEBUG(FED, "Received handshake request from federate %d", payload->neighbor_index);
    // Received a handshake request from a neighbor. Respond with our current state.
    self->neighbor_state[payload->neighbor_index].handshake_request_received = true;
    FederateMessage *msg = &env_fed->net_bundles[payload->neighbor_index]->send_msg;
    NetworkChannel *chan = env_fed->net_bundles[payload->neighbor_index]->net_channel;
    msg->which_message = FederateMessage_startup_coordination_tag;
    msg->message.startup_coordination.which_message = StartupCoordination_startup_handshake_response_tag;
    msg->message.startup_coordination.message.startup_handshake_response.state = self->state;

    // If we are in handshaking mode, then we send until we get the ACK, to ensure correct startup
    // If we are in another mode, we dont keep repeating because we dont want to block our execution.
    if (self->state == StartupCoordinationState_HANDSHAKING) {
      do {
        ret = chan->send_blocking(chan, msg);
      } while (ret != LF_OK && self->state);
    } else {
      ret = chan->send_blocking(chan, msg);
      if (ret != LF_OK) {
        LF_WARN(FED, "Failed to send handshake response to neighbor %d. Dropping request and wait for next.",
                payload->neighbor_index);
      }
    }
  }
}

/** Handle external handshake responses. */
static void StartupCoordinator_handle_startup_handshake_response(StartupCoordinator *self, StartupEvent *payload) {
  LF_DEBUG(FED, "Received handshake response from federate %d", payload->neighbor_index);
  switch (self->state) {
  case StartupCoordinationState_CONNECTING:
  case StartupCoordinationState_UNINITIALIZED:
    validate(false); // Should not be possible
    break;
  case StartupCoordinationState_NEGOTIATING:
  case StartupCoordinationState_RUNNING:
    // This could happen if our network channel can have duplicates.
    validate(false);
    break;
  case StartupCoordinationState_HANDSHAKING: {
    self->neighbor_state[payload->neighbor_index].handshake_response_received = true;

    // Check if we have received responses from all neighbors and thus completed the handshake.
    bool all_received = true;
    for (size_t i = 0; i < self->num_neighbours; i++) {
      if (!self->neighbor_state[i].handshake_response_received) {
        all_received = false;
        break;
      }
    }

    if (all_received) {
      LF_INFO(FED, "Handshake completed with %zu federated peers", self->num_neighbours);
      self->state = StartupCoordinationState_NEGOTIATING;
      // Schedule the start time negotiation to occur immediately.
      StartupCoordinator_schedule_system_self_event(self, 0, StartupCoordination_start_time_proposal_tag);
    }
    break;
  }
  }
}

/** Convenience function to send out a start time proposal to all neighbors for a step. */
static void send_start_time_proposal(StartupCoordinator *self, instant_t start_time, int step) {
  lf_ret_t ret;
  EnvironmentFederated *env_fed = (EnvironmentFederated *)self->env;
  LF_DEBUG(FED, "Sending start time proposal " PRINTF_TIME " step %d to all neighbors", start_time, step);
  for (size_t i = 0; i < self->num_neighbours; i++) {
    NetworkChannel *chan = env_fed->net_bundles[i]->net_channel;
    self->msg.which_message = FederateMessage_startup_coordination_tag;
    self->msg.message.startup_coordination.which_message = StartupCoordination_start_time_proposal_tag;
    self->msg.message.startup_coordination.message.start_time_proposal.time = start_time;
    self->msg.message.startup_coordination.message.start_time_proposal.step = step;
    do {
      ret = chan->send_blocking(chan, &self->msg);
    } while (ret != LF_OK);
  }
}

/** Handle a start time proposal, either from self or from neighbor. */
static void StartupCoordinator_handle_start_time_proposal(StartupCoordinator *self, StartupEvent *payload) {
  EnvironmentFederated *env_fed = (EnvironmentFederated *)self->env;
  if (payload->neighbor_index == NEIGHBOR_INDEX_SELF) {
    LF_DEBUG(FED, "Received start time proposal from self");
    switch (self->state) {
    case StartupCoordinationState_NEGOTIATING: {
      // An event scheduled locally, means that we should start the start time negotiation.
      validate(self->start_time_proposal_step == 0); // We should not have started the negotiation yet.
      self->start_time_proposal_step = 1;
      // Calculate and broadcast our own initial proposal.
      instant_t my_proposal;
      if ((env_fed->do_clock_sync && env_fed->clock_sync->is_grandmaster) || !env_fed->do_clock_sync) {
        my_proposal = self->env->get_physical_time(self->env) + (MSEC(100) * self->longest_path);
      } else {
        my_proposal = NEVER;
      }
      send_start_time_proposal(self, my_proposal, self->start_time_proposal_step);

      // We might have already received a proposal from a neighbor so we need to compare it with our own,
      // and possibly update our own proposal.
      if (my_proposal > self->start_time_proposal) {
        self->start_time_proposal = my_proposal;
      }
    } break;
    default:
      validate(false);
    }

  } else {
    // Received an external start time proposal.
    instant_t proposed_time = payload->msg.message.start_time_proposal.time;
    size_t step = payload->msg.message.start_time_proposal.step;
    LF_DEBUG(FED, "Received start time proposal " PRINTF_TIME " step %d from federate %d", proposed_time, step,
             payload->neighbor_index);
    switch (self->state) {
    case StartupCoordinationState_UNINITIALIZED:
    case StartupCoordinationState_CONNECTING:
    case StartupCoordinationState_RUNNING:
      // Should not be possible.
      validate(false);
    case StartupCoordinationState_HANDSHAKING:
      // This is possible. Our node might be still handshaking with another neighbor.
      // Intentional fall-through
    case StartupCoordinationState_NEGOTIATING: {
      // Update the number of proposals received from this neighbor and verify that the step is correct.
      self->neighbor_state[payload->neighbor_index].start_time_proposals_received++;
      validate(self->neighbor_state[payload->neighbor_index].start_time_proposals_received == step);
      // Update the start time if the received proposal is larger than the current.
      if (payload->msg.message.start_time_proposal.time > self->start_time_proposal) {
        LF_DEBUG(FED, "Start time proposal from federate %d is larger than current, updating.",
                 payload->neighbor_index);
        self->start_time_proposal = payload->msg.message.start_time_proposal.time;
      }
    } break;
    }
  }

  // We check for completion of an iteration both after receiving an external start time proposal,
  // and after sending out our initial one.

  // Check if we have received all proposals in the current iteration.
  bool iteration_completed = true;
  for (size_t i = 0; i < self->num_neighbours; i++) {
    if (self->neighbor_state[i].start_time_proposals_received < self->start_time_proposal_step) {
      iteration_completed = false;
      break;
    }
  }

  // There is an edge case at the very first step. We might have received start time proposal from
  // both our neighbors without having sent out our first yet. Then we dont complete the iteration yet.
  if (self->start_time_proposal_step == 0) {
    iteration_completed = false;
  }

  if (iteration_completed) {
    LF_INFO(FED, "Start time negotiation round %d completed. Current start time: " PRINTF_TIME,
            self->start_time_proposal_step, self->start_time_proposal);
    if (self->start_time_proposal_step == self->longest_path) {
      LF_INFO(FED, "Start time negotiation completed Starting at " PRINTF_TIME, self->start_time_proposal);
      self->state = StartupCoordinationState_RUNNING;
      self->env->scheduler->set_and_schedule_start_tag(self->env->scheduler, self->start_time_proposal);
    } else {
      self->start_time_proposal_step++;
      send_start_time_proposal(self, self->start_time_proposal, self->start_time_proposal_step);
    }
  }
}

/** Invoked by scheduler when handling any system event destined for StartupCoordinator. */
static void StartupCoordinator_handle_system_event(SystemEventHandler *_self, SystemEvent *event) {
  StartupCoordinator *self = (StartupCoordinator *)_self;
  StartupEvent *payload = (StartupEvent *)event->super.payload;
  switch (payload->msg.which_message) {
  case StartupCoordination_startup_handshake_request_tag:
    StartupCoordinator_handle_startup_handshake_request(self, payload);
    break;

  case StartupCoordination_startup_handshake_response_tag:
    StartupCoordinator_handle_startup_handshake_response(self, payload);
    break;

  case StartupCoordination_start_time_proposal_tag:
    StartupCoordinator_handle_start_time_proposal(self, payload);
    break;

  case StartupCoordination_start_time_response_tag:
    // This message is needed for transient federates. It should be implemented by putting an event on
    // the system event queue.
    LF_DEBUG(FED, "Received start time response from federate %d", payload->neighbor_index);
    validate(false);
    break;

  case StartupCoordination_start_time_request_tag:
    // This message is needed for transient federates. It should be implemented by putting an event on
    // the system event queue.
    LF_DEBUG(FED, "Received start time request from federate %d", payload->neighbor_index);
    validate(false);
    break;
  }

  _self->payload_pool.free(&_self->payload_pool, event->super.payload);
}

void StartupCoordinator_start(StartupCoordinator *self) {
  StartupCoordinator_schedule_system_self_event(self, 0, StartupCoordination_startup_handshake_request_tag);
}

void StartupCoordinator_ctor(StartupCoordinator *self, Environment *env, NeighborState *neighbor_state,
                             size_t num_neighbors, size_t longest_path, size_t payload_size, void *payload_buf,
                             bool *payload_used_buf, size_t payload_buf_capacity) {
  validate(!(longest_path == 0 && num_neighbors > 0));
  self->env = env;
  self->longest_path = longest_path;
  self->state = StartupCoordinationState_UNINITIALIZED;
  self->neighbor_state = neighbor_state;
  self->num_neighbours = num_neighbors;
  self->longest_path = longest_path;
  self->start_time_proposal_step = 0;
  self->start_time_proposal = NEVER;
  for (size_t i = 0; i < self->num_neighbours; i++) {
    self->neighbor_state[i].handshake_response_received = false;
    self->neighbor_state[i].handshake_request_received = false;
    self->neighbor_state[i].handshake_response_sent = false;
    self->neighbor_state[i].start_time_proposals_received = 0;
  }

  self->handle_message_callback = StartupCoordinator_handle_message_callback;
  self->start = StartupCoordinator_start;
  self->connect_to_neighbors_blocking = StartupCoordinator_connect_to_neighbors_blocking;
  self->super.handle = StartupCoordinator_handle_system_event;
  EventPayloadPool_ctor(&self->super.payload_pool, payload_buf, payload_used_buf, payload_size, payload_buf_capacity,
                        NUM_RESERVED_EVENTS);
}