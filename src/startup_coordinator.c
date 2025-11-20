#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/logging.h"
#include "proto/message.pb.h"

#include <reactor-uc/timer.h>

#define NEIGHBOR_INDEX_SELF -1
#define NUM_RESERVED_EVENTS 3 // 3 events is reserved for scheduling our own events.

#ifndef TRANSIENT_WAIT_TIME
#define TRANSIENT_WAIT_TIME MSEC(250)
#endif

/**
 * @brief Open connections to all neighbors. This function will block until all connections are established.
 */
static lf_ret_t StartupCoordinator_connect_to_neighbors_blocking(StartupCoordinator* self) {
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  validate(self->state == StartupCoordinationState_UNINITIALIZED);
  self->state = StartupCoordinationState_CONNECTING;
  LF_DEBUG(FED, "%s connecting to %zu federated peers", self->env->main->name, env_fed->net_bundles_size);
  lf_ret_t ret;

  // Open all connections.
  for (size_t i = 0; i < env_fed->net_bundles_size; i++) {
    FederatedConnectionBundle* bundle = env_fed->net_bundles[i];
    NetworkChannel* chan = bundle->net_channel;
    ret = chan->open_connection(chan);
    // If we cannot open our network channels, then we cannot proceed and must abort.
    validate(ret == LF_OK);
  }

  bool all_connected = false;
  interval_t wait_before_retry = NEVER;
  while (!all_connected) {
    // Wait time initialized to minimum value so we can find the maximum.
    all_connected = true;
    for (size_t i = 0; i < self->num_neighbours; i++) {
      NetworkChannel* chan = env_fed->net_bundles[i]->net_channel;
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
      self->env->wait_until(self->env, self->env->get_physical_time(self->env) + wait_before_retry);
    }
  }

  LF_DEBUG(FED, "%s Established connection to all %zu federated peers", self->env->main->name,
           env_fed->net_bundles_size);
  self->state = StartupCoordinationState_HANDSHAKING;
  return LF_OK;
}

void StartupCoordinator_schedule_timers_joining(StartupCoordinator* self, Reactor* reactor,
                                                interval_t federation_start_time, interval_t join_time) {
  lf_ret_t ret;
  for (size_t i = 0; i < reactor->triggers_size; i++) {
    Trigger* trigger = reactor->triggers[i];
    if (trigger->type == TRIG_TIMER) {
      Timer* timer = (Timer*)trigger;
      const interval_t duration = join_time - federation_start_time - timer->offset;
      const interval_t individual_join_time = ((duration / timer->period) + 1) * timer->period + federation_start_time;
      tag_t tag = {.time = individual_join_time + timer->offset, .microstep = 0};
      Event event = EVENT_INIT(tag, &timer->super, NULL);
      ret = self->env->scheduler->schedule_at(self->env->scheduler, &event);
      validate(ret == LF_OK);
    }
  }
  for (size_t i = 0; i < reactor->children_size; i++) {
    StartupCoordinator_schedule_timers_joining(self, reactor->children[i], federation_start_time, join_time);
  }
}

/** Schedule a system-event with `self` as the origin for some future time. */
static void StartupCoordinator_schedule_system_self_event(StartupCoordinator* self, instant_t time, int message_type) {
  StartupEvent* payload = NULL;
  lf_ret_t ret;
  // Allocate one of the reserved events for our own use.
  ret = self->super.payload_pool.allocate_reserved(&self->super.payload_pool, (void**)&payload);

  if (ret != LF_OK) {
    LF_ERR(FED, "Failed to allocate payload for startup system event.");
    // This is a critical error as we should have enough events reserved for our own use.
    validate(false);
    return;
  }

  payload->neighbor_index = NEIGHBOR_INDEX_SELF;
  payload->msg.which_message = message_type;
  tag_t tag = {.time = time, .microstep = 0};

  SystemEvent event = SYSTEM_EVENT_INIT(tag, &self->super, payload);
  ret = self->env->scheduler->schedule_system_event_at(self->env->scheduler, &event);
  if (ret != LF_OK) {
    LF_ERR(FED, "Failed to schedule startup system event.");
    validate(false);
  }
}

/** Handle an incoming message from the network. Invoked from an async context in a critical section. */
static void StartupCoordinator_handle_message_callback(StartupCoordinator* self, const StartupCoordination* msg,
                                                       size_t bundle_idx) {
  LF_DEBUG(FED, "Received startup message from neighbor %zu. Scheduling as a system event", bundle_idx);
  ClockSyncEvent* payload = NULL;
  lf_ret_t ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void**)&payload);
  if (ret == LF_OK) {
    payload->neighbor_index = bundle_idx;
    memcpy(&payload->msg, msg, sizeof(StartupCoordination));
    tag_t tag = {.time = self->env->get_physical_time(self->env), .microstep = 0};
    SystemEvent event = SYSTEM_EVENT_INIT(tag, &self->super, payload);
    ret = self->env->scheduler->schedule_system_event_at(self->env->scheduler, &event);
    if (ret != LF_OK) {
      // Critical error, there should be place in the system event queue if we could allocate a payload.
      LF_ERR(FED, "Failed to schedule startup system event.");
      validate(false);
    }
  } else {
    LF_ERR(FED, "Failed to allocate payload for incoming startup coordination message. Dropping it");
  }
}

/** Handle a request, either local or external, to do a startup handshake. This is called from the runtime context. */
static void StartupCoordinator_handle_startup_handshake_request(StartupCoordinator* self, StartupEvent* payload) {
  lf_ret_t ret;
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  if (payload->neighbor_index == NEIGHBOR_INDEX_SELF) {
    LF_DEBUG(FED, "Received handshake request from self");
    switch (self->state) {
    case StartupCoordinationState_HANDSHAKING:
      for (size_t i = 0; i < self->num_neighbours; i++) {
        FederateMessage* msg = &env_fed->net_bundles[i]->send_msg;
        NetworkChannel* chan = env_fed->net_bundles[i]->net_channel;

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
    FederateMessage* msg = &env_fed->net_bundles[payload->neighbor_index]->send_msg;
    NetworkChannel* chan = env_fed->net_bundles[payload->neighbor_index]->net_channel;
    msg->which_message = FederateMessage_startup_coordination_tag;
    msg->message.startup_coordination.which_message = StartupCoordination_startup_handshake_response_tag;
    msg->message.startup_coordination.message.startup_handshake_response.state = self->state;

    LF_DEBUG(FED, "We are currently in %d mode", self->state);
    // If we are in handshaking mode, then we send until we get the ACK, to ensure correct startup
    // If we are in another mode, we dont keep repeating because we dont want to block our execution.
    if (self->state == StartupCoordinationState_HANDSHAKING) {
      do {
        ret = chan->send_blocking(chan, msg);
      } while (ret != LF_OK);
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
static void StartupCoordinator_handle_startup_handshake_response(StartupCoordinator* self, StartupEvent* payload) {
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
    self->neighbor_state[payload->neighbor_index].initial_state_of_neighbor =
        payload->msg.message.startup_handshake_response.state;

    // Check if we have received responses from all neighbors and thus completed the handshake.
    bool all_received = true;
    for (size_t i = 0; i < self->num_neighbours; i++) {
      if (!self->neighbor_state[i].handshake_response_received) {
        all_received = false;
        break;
      }
    }

    if (all_received) {
      LF_DEBUG(FED, "Handshake completed with %zu federated peers", self->num_neighbours);
      self->state = StartupCoordinationState_NEGOTIATING;

      bool during_startup = true;
      bool all_running = true;
      for (size_t i = 0; i < self->num_neighbours; i++) {
        LF_DEBUG(FED, "State of neighbor %i : %i", i, self->neighbor_state[i].initial_state_of_neighbor);
        all_running =
            all_running && self->neighbor_state[i].initial_state_of_neighbor == StartupCoordinationState_RUNNING;
        during_startup = during_startup &&
                         (self->neighbor_state[i].initial_state_of_neighbor == StartupCoordinationState_HANDSHAKING ||
                          self->neighbor_state[i].initial_state_of_neighbor == StartupCoordinationState_NEGOTIATING ||
                          self->neighbor_state[i].initial_state_of_neighbor == StartupCoordinationState_UNINITIALIZED);
      }

      if (all_running) {
        // It appears that this federate is a transient federate.
        // Requesting the start tag from neighboring federates.
        StartupCoordinator_schedule_system_self_event(self, self->env->get_physical_time(self->env) + MSEC(50),
                                                      StartupCoordination_start_time_request_tag);
      } else if (during_startup) {
        // Schedule the start time negotiation to occur immediately.
        StartupCoordinator_schedule_system_self_event(self, self->env->get_physical_time(self->env) + MSEC(50),
                                                      StartupCoordination_start_time_proposal_tag);
      } else {
        LF_ERR(FED, "Some neighbors are running some are not initialized! Cannot startup!");
        validate(false);
      }
    }
    break;
  }
  }
}

/** Convenience function to send out a start time proposal to all neighbors for a step. */
static void send_start_time_proposal(StartupCoordinator* self, instant_t start_time, int step) {
  lf_ret_t ret;
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
  LF_DEBUG(FED, "Sending start time proposal " PRINTF_TIME " step %d to all neighbors", start_time, step);
  for (size_t i = 0; i < self->num_neighbours; i++) {
    NetworkChannel* chan = env_fed->net_bundles[i]->net_channel;
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
static void StartupCoordinator_handle_start_time_proposal(StartupCoordinator* self, StartupEvent* payload) {
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->env;
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
      break;
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
    LF_DEBUG(FED, "Start time negotiation round %d completed. Current start time: " PRINTF_TIME,
             self->start_time_proposal_step, self->start_time_proposal);
    if (self->start_time_proposal_step == self->longest_path) {
      LF_INFO(FED, "Start time negotiation completed Starting at " PRINTF_TIME, self->start_time_proposal);
      self->state = StartupCoordinationState_RUNNING;
      self->env->scheduler->set_and_schedule_start_tag(self->env->scheduler, self->start_time_proposal);
      tag_t start_tag = {.time = self->start_time_proposal, .microstep = 0};
      Environment_schedule_startups(self->env, start_tag);
      Environment_schedule_timers(self->env, self->env->main, start_tag);
    } else {
      self->start_time_proposal_step++;
      send_start_time_proposal(self, self->start_time_proposal, self->start_time_proposal_step);
    }
  }
}

static void StartupCoordinator_handle_start_time_request(StartupCoordinator* self, StartupEvent* payload) {
  FederatedEnvironment* env = (FederatedEnvironment*)self->env;
  if (payload->neighbor_index == NEIGHBOR_INDEX_SELF) {
    for (size_t i = 0; i < self->num_neighbours; i++) {
      NetworkChannel* chan = env->net_bundles[i]->net_channel;
      self->msg.which_message = FederateMessage_startup_coordination_tag;
      self->msg.message.startup_coordination.which_message = StartupCoordination_start_time_request_tag;
      lf_ret_t ret;
      do {
        ret = chan->send_blocking(chan, &self->msg);
      } while (ret != LF_OK);

      // We now schedule a system event here, because otherwise we will never detect no other federates responding
      StartupCoordinator_schedule_system_self_event(self, self->env->get_physical_time(self->env) + TRANSIENT_WAIT_TIME,
                                                    StartupCoordination_start_time_response_tag);
    }

  } else {
    switch (self->state) {
    case StartupCoordinationState_RUNNING: {
      FederateMessage* msg = &env->net_bundles[payload->neighbor_index]->send_msg;
      NetworkChannel* chan = env->net_bundles[payload->neighbor_index]->net_channel;
      msg->which_message = FederateMessage_startup_coordination_tag;
      msg->message.startup_coordination.which_message = StartupCoordination_start_time_response_tag;
      msg->message.startup_coordination.message.start_time_response.elapsed_logical_time =
          self->env->get_elapsed_logical_time(self->env);
      msg->message.startup_coordination.message.start_time_response.federation_start_time = self->start_time_proposal;
      chan->send_blocking(chan, msg);
      LF_INFO(FED, "SENDING TIME start_tag: " PRINTF_TIME " elapsed_time: " PRINTF_TIME,
              msg->message.startup_coordination.message.start_time_response.federation_start_time,
              msg->message.startup_coordination.message.start_time_response.elapsed_logical_time);
      break;
    }
    default:;
    }
  }
}

static void StartupCoordinator_handle_start_time_response(StartupCoordinator* self, StartupEvent* payload) {
  if (self->start_time_proposal > 0) {
    return;
  }

  if (payload->neighbor_index == NEIGHBOR_INDEX_SELF) {
    bool got_no_response = true;
    for (size_t i = 0; i < self->num_neighbours; i++) {
      got_no_response = got_no_response && (self->neighbor_state[i].current_logical_time == 0);
    }

    if (got_no_response) {
      LF_ERR(FED, "No other federate responded to the start time request! Shutting Down!");
      self->env->request_shutdown(self->env);
    }
  }

  const instant_t start_time = payload->msg.message.start_time_response.federation_start_time;
  const interval_t elapsed_logical = payload->msg.message.start_time_response.elapsed_logical_time;
  const instant_t current_logical_time = start_time + elapsed_logical;
  validate(start_time < current_logical_time);

  self->neighbor_state[payload->neighbor_index].current_logical_time = current_logical_time;
  self->neighbor_state[payload->neighbor_index].start_time_proposals_received++;

  // checking if we got a response from all core federates
  for (size_t i = 0; i < self->num_neighbours; i++) {
    if (self->neighbor_state[i].core_federate && self->neighbor_state[i].current_logical_time == 0) {
      return;
    }
  }

  // calculating the maximum logical time from all neighbors that responded
  interval_t max_logical_time = 0;
  for (size_t i = 0; i < self->num_neighbours; i++) {
    if (self->neighbor_state[i].current_logical_time > max_logical_time) {
      max_logical_time = self->neighbor_state[i].current_logical_time;
    }
  }

  self->start_time_proposal = start_time;
  self->state = StartupCoordinationState_RUNNING;

  instant_t joining_time = 0;

  if (self->joining_policy == JOIN_IMMEDIATELY) {
    joining_time = max_logical_time + MSEC(50);
    tag_t start_tag = {.time = joining_time, .microstep = 0};
    LF_INFO(FED, "Policy: IMMEDIATELY Scheduling join_time: " PRINTF_TIME, joining_time);
    self->env->scheduler->prepare_timestep(self->env->scheduler, NEVER_TAG);
    Environment_schedule_startups(self->env, start_tag);
    Environment_schedule_timers(self->env, self->env->main, start_tag);
    self->env->scheduler->prepare_timestep(self->env->scheduler, start_tag);
    self->env->scheduler->set_and_schedule_start_tag(self->env->scheduler, joining_time);
  } else if (self->joining_policy == JOIN_TIMER_ALIGNED) {
    joining_time = max_logical_time + MSEC(50);
    tag_t start_tag = {.time = joining_time, .microstep = 0};
    LF_INFO(FED, "Policy: Timer Aligned Scheduling join_time: " PRINTF_TIME, joining_time);
    self->env->scheduler->prepare_timestep(self->env->scheduler, NEVER_TAG);
    Environment_schedule_startups(self->env, start_tag);
    StartupCoordinator_schedule_timers_joining(self, self->env->main, start_time, joining_time);
    self->env->scheduler->prepare_timestep(self->env->scheduler, start_tag);
    self->env->scheduler->set_and_schedule_start_tag(self->env->scheduler, joining_time);
  } else {
    validate(false);
  }
}

static void StartupCoordinator_handle_join_time_announcement(const StartupCoordinator* self,
                                                             const StartupEvent* payload) {
  if (payload->neighbor_index != NEIGHBOR_INDEX_SELF) {

    const FederatedEnvironment* env = (FederatedEnvironment*)self->env;
    for (size_t i = 0; i < env->net_bundles_size; i++) {
      if (env->net_bundles[i]->index == (size_t)payload->neighbor_index) {

        // we found the correct connection bundle to this federate now we set last known tag to the joining time.
        const FederatedConnectionBundle* bundle = env->net_bundles[i];
        for (size_t j = 0; j < bundle->inputs_size; j++) {
          tag_t joining_time = {.time = payload->msg.message.joining_time_announcement.joining_time, .microstep = 0};
          bundle->inputs[i]->last_known_tag = joining_time;
        }
      }
    }
  }
}

/** Invoked by scheduler when handling any system event destined for StartupCoordinator. */
static void StartupCoordinator_handle_system_event(SystemEventHandler* _self, SystemEvent* event) {
  StartupCoordinator* self = (StartupCoordinator*)_self;
  StartupEvent* payload = (StartupEvent*)event->super.payload;
  switch (payload->msg.which_message) {
  case StartupCoordination_startup_handshake_request_tag:
    LF_INFO(FED, "Handle: Handshake Reqeust Tag System Event");
    StartupCoordinator_handle_startup_handshake_request(self, payload);
    break;

  case StartupCoordination_startup_handshake_response_tag:
    LF_INFO(FED, "Handle: Handshake Response Tag System Event");
    StartupCoordinator_handle_startup_handshake_response(self, payload);
    break;

  case StartupCoordination_start_time_proposal_tag:
    LF_INFO(FED, "Handle: Start Time Proposal Tag System Event");
    StartupCoordinator_handle_start_time_proposal(self, payload);
    break;

  case StartupCoordination_start_time_response_tag:
    LF_INFO(FED, "Handle: Start Time Response System Event");
    StartupCoordinator_handle_start_time_response(self, payload);
    break;

  case StartupCoordination_start_time_request_tag:
    LF_INFO(FED, "Handle: Start Time Request Tag System Event");
    StartupCoordinator_handle_start_time_request(self, payload);
    break;

  case StartupCoordination_joining_time_announcement_tag:
    LF_INFO(FED, "Handle: Joining Time Announcement");
    StartupCoordinator_handle_join_time_announcement(self, payload);
    break;
  }

  _self->payload_pool.free(&_self->payload_pool, event->super.payload);
}

void StartupCoordinator_start(StartupCoordinator* self) {
  StartupCoordinator_schedule_system_self_event(self, self->env->get_physical_time(self->env) + MSEC(250),
                                                StartupCoordination_startup_handshake_request_tag);
}

void StartupCoordinator_ctor(StartupCoordinator* self, Environment* env, NeighborState* neighbor_state,
                             size_t num_neighbors, size_t longest_path, JoiningPolicy joining_policy,
                             size_t payload_size, void* payload_buf, bool* payload_used_buf,
                             size_t payload_buf_capacity) {
  validate(!(longest_path == 0 && num_neighbors > 0));
  self->env = env;
  self->longest_path = longest_path;
  self->state = StartupCoordinationState_UNINITIALIZED;
  self->neighbor_state = neighbor_state;
  self->num_neighbours = num_neighbors;
  self->longest_path = longest_path;
  self->start_time_proposal_step = 0;
  self->start_time_proposal = NEVER;
  self->joining_policy = joining_policy;
  for (size_t i = 0; i < self->num_neighbours; i++) {
    self->neighbor_state[i].core_federate = true;
    self->neighbor_state[i].current_logical_time = 0;
    self->neighbor_state[i].handshake_response_received = false;
    self->neighbor_state[i].handshake_request_received = false;
    self->neighbor_state[i].handshake_response_sent = false;
    self->neighbor_state[i].start_time_proposals_received = 0;
  }

  self->handle_message_callback = StartupCoordinator_handle_message_callback;
  self->start = StartupCoordinator_start;
  self->connect_to_neighbors_blocking = StartupCoordinator_connect_to_neighbors_blocking;
  self->super.handle = StartupCoordinator_handle_system_event;
  EventPayloadPool_ctor(&self->super.payload_pool, (char*)payload_buf, payload_used_buf, payload_size,
                        payload_buf_capacity, NUM_RESERVED_EVENTS);
}
