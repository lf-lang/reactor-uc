#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/logging.h"
#include "proto/message.pb.h"

/**
 * @brief A reusable function for waiting on all neighbors to meet a certain condition.
 *
 * This function will wait until all neighbors have reached a certain state, as defined by the condition function ptr.
 * If a neighbor has not reached the state, the retry function will be called. Before checking the condition
 * again a wait time will be calculated based on the expected_connect_duration of the channels.
 * Used for establishing connection to neighbors, for performing a handshake and for negotiating the start tag.
 *
 * @param self
 * @param condition_locked A function that takes the coordinator and the index of the neighbor and returns true if the
 * neighbor has reached the state. Called from a critical section.
 * @param retry_locked A function that takes the coordinator and the index of the neighbor and retries the action.
 * Called from a critical section.
 */
static void wait_for_neighbors_state_with_timeout_locked(StartupCoordinator *self,
                                                         bool (*condition_locked)(StartupCoordinator *self, size_t idx),
                                                         void (*retry_locked)(StartupCoordinator *self, size_t idx)) {
  bool all_conditions_met = false;
  while (!all_conditions_met) {
    for (int i = 0; i < (int)self->env->net_bundles_size; i++) {
      if (self->env->net_bundles[i]->net_channel->mode == NETWORK_CHANNEL_MODE_POLLED) {
        ((PolledNetworkChannel *)self->env->net_bundles[i]->net_channel)
            ->poll((PolledNetworkChannel *)self->env->net_bundles[i]->net_channel);
      }
    }

    // Wait time initialized to minimum value so we can find the maximum.
    interval_t wait_before_retry = NEVER;
    all_conditions_met = true;
    for (size_t i = 0; i < self->num_neighbours; i++) {
      NetworkChannel *chan = self->env->net_bundles[i]->net_channel;
      // Check whether the neighbor has reached the desired state.
      if (!condition_locked(self, i)) {
        // If a retry function is provided, call it.
        if (retry_locked) {
          retry_locked(self, i);
        }
        all_conditions_met = false;
        // Check if the expected_connect_duration is longer than the current wait time.
        if (chan->expected_connect_duration > wait_before_retry) {
          wait_before_retry = chan->expected_connect_duration;
        }
      }
    }

    if (!all_conditions_met) {
      // This will release the critical section and allow other tasks to run.
      self->env->wait_until(self->env, self->env->get_physical_time(self->env) + wait_before_retry);
    }
  }
}
/**
 * @brief Check that the network channel is in the connected state.
 */
static bool connect_condition_locked(StartupCoordinator *self, size_t idx) {
  return self->env->net_bundles[idx]->net_channel->is_connected(self->env->net_bundles[idx]->net_channel);
}

/**
 * @brief Open connections to all neighbors. This function will block until all connections are established.
 */
static lf_ret_t StartupCoordinator_connect_to_neighbors(StartupCoordinator *self) {
  self->env->enter_critical_section(self->env);
  validate(self->state == StartupCoordinationState_UNINITIALIZED);
  self->state = StartupCoordinationState_CONNECTING;
  LF_INFO(FED, "%s connecting to %zu federated peers", self->env->main->name, self->env->net_bundles_size);
  lf_ret_t ret;

  // Ope
  self->env->leave_critical_section(self->env);
  // Open all connections.
  for (size_t i = 0; i < self->env->net_bundles_size; i++) {
    FederatedConnectionBundle *bundle = self->env->net_bundles[i];
    NetworkChannel *chan = bundle->net_channel;
    ret = chan->open_connection(chan);
    validate(ret == LF_OK);
  }
  self->env->enter_critical_section(self->env);

  // Block until all connections are established. The critical section will be left during the wait.
  wait_for_neighbors_state_with_timeout_locked(self, connect_condition_locked, NULL);
  self->env->leave_critical_section(self->env);

  LF_INFO(FED, "%s Established connection to all %zu federated peers", self->env->main->name,
          self->env->net_bundles_size);
  return LF_OK;
}

/**
 * @brief A handshake is done when we have received and responded to a request from the peer,
 * and also sent a request and received a response from the peer.
 */
static bool handshake_condition_locked(StartupCoordinator *self, size_t idx) {
  return self->neighbor_state[idx].handshake_response_received && self->neighbor_state[idx].handshake_response_sent &&
         self->neighbor_state[idx].handshake_request_received;
}

/**
 * @brief Check for received requests and respond.
 */
static void handshake_retry_locked(StartupCoordinator *self, size_t idx) {
  if (self->neighbor_state[idx].handshake_request_received && !self->neighbor_state[idx].handshake_response_sent) {
    NetworkChannel *chan = self->env->net_bundles[idx]->net_channel;
    self->msg.which_message = FederateMessage_startup_coordination_tag;
    self->msg.message.startup_coordination.which_message = StartupCoordination_startup_handshake_response_tag;
    self->msg.message.startup_coordination.message.startup_handshake_response.state = self->state;
    self->neighbor_state[idx].handshake_response_sent = true;
    self->env->leave_critical_section(self->env);
    chan->send_blocking(chan, &self->msg);
    self->env->enter_critical_section(self->env);
  }
}

/**
 * @brief Perform a handshake with all neighbors.
 *
 * This function is blocking and returns once a handshake has been performed with all neighbors.
 * It first sends a handshake request to all neighbors and then waits for all neighbors to respond.
 * It also responds to handshake requests from neighbors.
 */
static lf_ret_t StartupCoordinator_perform_handshake(StartupCoordinator *self) {
  LF_INFO(FED, "%s performing handshake with %zu federated peers", self->env->main->name, self->env->net_bundles_size);
  self->env->enter_critical_section(self->env);

  validate(self->state == StartupCoordinationState_CONNECTING);
  self->state = StartupCoordinationState_HANDSHAKING;
  self->env->leave_critical_section(self->env);
  // Send handshake requests to all neighbors.
  for (size_t i = 0; i < self->env->net_bundles_size; i++) {
    NetworkChannel *chan = self->env->net_bundles[i]->net_channel;
    self->msg.which_message = FederateMessage_startup_coordination_tag;
    self->msg.message.startup_coordination.which_message = StartupCoordination_startup_handshake_request_tag;
    chan->send_blocking(chan, &self->msg);
  }

  self->env->enter_critical_section(self->env);
  // Wait for all neighbors to respond to the handshake request, also handle incoming requests.

  wait_for_neighbors_state_with_timeout_locked(self, handshake_condition_locked, handshake_retry_locked);
  self->env->leave_critical_section(self->env);
  LF_INFO(FED, "%s Handshake completed with %zu federated peers", self->env->main->name, self->env->net_bundles_size);
  return LF_OK;
}

/**
 * @brief The condition for completing a start tag negotiation step is that we have
 * received a number of start time proposal from the peer which is greater than or equal to the current step.
 *
 */
static bool start_tag_condition_locked(StartupCoordinator *self, size_t idx) {
  return self->neighbor_state[idx].start_time_proposals_received >= self->start_time_proposal_step;
}

/**
 * @brief Negotiate the start time with all neighbors. This function will block until a
 * start time has been agreed upon.
 *
 * This function proceeds in several steps. In each step, the federates send out their current proposal
 * for the start time and wait until they have received proposals from all neighbors. The federates
 * update their proposal if they receive a proposal that is larger than their current proposal.
 * The process is repeated for as many steps as the longest path in the network, this will ensure that
 * the federates have a consistent view of the start time before starting.
 */
static instant_t StartupCoordinator_negotiate_start_time(StartupCoordinator *self) {
  validate(self->state == StartupCoordinationState_HANDSHAKING);
  self->env->enter_critical_section(self->env);
  self->state = StartupCoordinationState_NEGOTIATING;

  // This federate will propose a start time that is its current physical time plus an offset based on the
  // longest path to any other federate. Note that we compare it to the current proposal because we
  // might have received a proposal from another federate already.
  interval_t my_propsal = self->env->get_physical_time(self->env) + (MSEC(100) * self->longest_path);
  if (my_propsal > self->start_time_proposal) {
    self->start_time_proposal = my_propsal;
  }

  for (size_t i = 0; i < self->longest_path; i++) {
    self->start_time_proposal_step = i + 1;
    LF_DEBUG(FED, "Sending oput start time proposal %d: " PRINTF_TIME, self->start_time_proposal_step,
             self->start_time_proposal);

    self->env->leave_critical_section(self->env);
    for (size_t j = 0; j < self->num_neighbours; j++) {
      NetworkChannel *chan = self->env->net_bundles[j]->net_channel;
      self->msg.which_message = FederateMessage_startup_coordination_tag;
      self->msg.message.startup_coordination.which_message = StartupCoordination_start_time_proposal_tag;
      self->msg.message.startup_coordination.message.start_time_proposal.time = self->start_time_proposal;
      self->msg.message.startup_coordination.message.start_time_proposal.step = self->start_time_proposal_step;
      chan->send_blocking(chan, &self->msg);
    }
    self->env->enter_critical_section(self->env);

    wait_for_neighbors_state_with_timeout_locked(self, start_tag_condition_locked, NULL);
    self->start_time_proposal_step = i;
    LF_DEBUG(FED, "Start time proposal after step %d: " PRINTF_TIME, i, self->start_time_proposal);
  }

  LF_INFO(FED, "Final start time proposal: " PRINTF_TIME, self->start_time_proposal);
  self->state = StartupCoordinationState_RUNNING;
  self->env->leave_critical_section(self->env);
  return self->start_time_proposal;
}

/**
 * @brief Callback registered with the network channel to handle incoming StartupCoordination messages.abort
 *
 * NOTE: This function is async and cannot block, so it cannot e.g. call send_blocking.
 *
 * @param self
 * @param msg
 * @param bundle_index
 */
static void StartupCoordinator_handle_message_callback(StartupCoordinator *self, const StartupCoordination *msg,
                                                       size_t bundle_index) {
  self->env->enter_critical_section(self->env);

  switch (msg->which_message) {
  case StartupCoordination_startup_handshake_request_tag: {
    LF_DEBUG(FED, "Received handshake request from federate %zu", bundle_index);
    validate(self->state == StartupCoordinationState_CONNECTING || self->state == StartupCoordinationState_HANDSHAKING);
    self->neighbor_state[bundle_index].handshake_request_received = true;
    break;
  }
  case StartupCoordination_startup_handshake_response_tag:
    LF_DEBUG(FED, "Received handshake response from federate %zu", bundle_index);
    validate(self->state == StartupCoordinationState_HANDSHAKING);
    self->neighbor_state[bundle_index].handshake_response_received = true;
    break;
  case StartupCoordination_start_time_proposal_tag:
    LF_DEBUG(FED, "Received start time proposal from federate %zu", bundle_index);
    validate(self->neighbor_state[bundle_index].handshake_response_received == true);
    validate(self->neighbor_state[bundle_index].handshake_response_sent == true);
    validate(self->state == StartupCoordinationState_HANDSHAKING ||
             self->state == StartupCoordinationState_NEGOTIATING);
    validate(msg->message.start_time_proposal.step ==
             (self->neighbor_state[bundle_index].start_time_proposals_received + 1));
    validate(msg->message.start_time_proposal.step <= (self->start_time_proposal_step + 1));
    self->neighbor_state[bundle_index].start_time_proposals_received = msg->message.start_time_proposal.step;
    if (msg->message.start_time_proposal.time > self->start_time_proposal) {
      LF_DEBUG(FED, "Start time proposal from federate %zu is larger than current, updating.", bundle_index);
      self->start_time_proposal = msg->message.start_time_proposal.time;
    }
    break;
  case StartupCoordination_start_time_request_tag:
    // This message is needed for transient federates. It should be implemented by putting an event on
    // the system event queue.
    validate(false);
    break;
  }
  self->env->platform->new_async_event(self->env->platform);
  self->env->leave_critical_section(self->env);
}

void StartupCoordinator_ctor(StartupCoordinator *self, Environment *env, NeighborState *neighbor_state,
                             size_t num_neighbors, size_t longest_path) {
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

  self->connect_to_neigbors = StartupCoordinator_connect_to_neighbors;
  self->perform_handshake = StartupCoordinator_perform_handshake;
  self->negotiate_start_time = StartupCoordinator_negotiate_start_time;
  self->handle_message_callback = StartupCoordinator_handle_message_callback;
}