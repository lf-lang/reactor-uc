#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/tag.h"
#include "proto/message.pb.h"

// FIXME: We need better error handling. Detect if a federate crashes during the startup phase
//  and then reconnects.

// FIXME: A problem seems to be that we the federates can be up to one step away from each other.
//  So we could receive a start_time_proposal while we are in handshaking. I think we have to handle
//  this from the perspective of the

// TODO: We probably want to move the connection stuff here also. and use this function as well.

void wait_for_neighbors_state_with_timeout(StartupCoordinator *self,
                                           bool (*condition)(StartupCoordinator *self, int idx),
                                           void (*retry)(StartupCoordinator *self, int idx)) {
  bool all_conditions_met = false;
  while (!all_conditions_met) {
    interval_t wait_before_retry = NEVER;
    all_conditions_met = true;
    for (int i = 0; i < self->num_neighbours; i++) {
      NetworkChannel *chan = self->env->net_bundles[i]->channel;
      if (!condition(self, i)) {
        if (retry) {
          retry(self, i);
        }
        all_conditions_met = false;
        if (chan->expected_connect_duration > wait_before_retry) {
          wait_before_retry = chan->expected_connect_duration;
        }
      }
    }

    if (!all_conditions_met) {
      self->env->wait_until(self->env, wait_before_retry);
    }
  }
}

bool handshake_condition(StartupCoordinator *self, int idx) {
  return self->neighbor_state[idx].handshake_received >= self->start_time_proposal_step;
}

void handshake_retry(StartupCoordinator *self, int idx) {
  NetworkChannel *chan = self->env->net_bundles[idx]->channel;
  self->neighbor_state[idx].handshake_requested = true;
  self->msg.which_message = FederateMessage_startup_coordination_tag;
  self->msg.message.startup_coordination.which_message = StartupCoordination_start_time_proposal_tag;
  self->msg.message.startup_coordination.message.start_time_proposal.time = self->start_time_proposal;
  chan->send_blocking(chan, &self->msg);
}

lf_ret_t StartupCoordinator_perform_handshake(StartupCoordinator *self) {
  self->env->enter_critical_section(self->env);
  self->state = STARTUP_COORDINATOR_STATE_HANDSHAKE;
  wait_for_neighbors_state_with_timeout(self, handshake_condition, handshake_retry);
  self->state = STARTUP_COORDINATOR_STATE_NEGOTIATE_START_TAG;
  self->env->leave_critical_section(self->env);
  return LF_OK;
}

bool start_tag_condition(StartupCoordinator *self, int idx) {
  return self->neighbor_state[idx].start_time_received_counter > self->start_time_proposal_step;
}

void start_tag_retry(StartupCoordinator *self, int idx) {
  (void)self;
  (void)idx;
}

instant_t StartupCoordinator_negotiate_start_tag(StartupCoordinator *self) {
  self->env->enter_critical_section(self->env);

  self->start_time_proposal = self->env->get_physical_time(self->env) + (MSEC(100) * self->longest_path);

  for (int i = 0; i < self->longest_path; i++) {

    for (int i = 0; i < self->num_neighbours; i++) {
      NetworkChannel *chan = self->env->net_bundles[i]->channel;
      self->msg.which_message = FederateMessage_startup_coordination_tag;
      self->msg.message.startup_coordination.which_message = StartupCoordination_start_time_proposal_tag;
      self->msg.message.startup_coordination.message.start_time_proposal.time = self->start_time_proposal;
      chan->send_blocking(chan, &self->msg);
    }

    wait_for_neighbors_state_with_timeout(self, start_tag_condition, NULL);
    LF_PRINT_DEBUG("Start time proposal after step %d: %" PRId64, i, self->start_time_proposal);
  }

  LF_PRINT_INFO("Final start time proposal: %" PRId64, self->start_time_proposal);
  self->state = STARTUP_COORDINATOR_STATE_DONE;
  self->env->leave_critical_section(self->env);
  return self->start_time_proposal;
}

void StartupCoordinator_handle_message_callback(StartupCoordinator *self, const StartupCoordination *msg,
                                                size_t bundle_index) {
  (void)self;
  (void)msg;
  self->env->enter_critical_section(self->env);

  switch (msg->which_message) {
  case StartupCoordination_startup_handshake_request_tag:
    NetworkChannel *chan = self->env->net_bundles[bundle_index]->channel;
    self->msg.which_message = FederateMessage_startup_coordination_tag;
    self->msg.message.startup_coordination.which_message = StartupCoordination_startup_handshake_response_tag;
    self->msg.message.message.startup_handshake_response.state = self->state;
    chan->send_blocking(chan, &self->msg);
    break;
  case StartupCoordination_startup_handshake_response_tag:
    self->neighbor_state[bundle_index].handshake_received = true;
    break;
  case StartupCoordination_start_time_proposal_tag:
    self->neighbor_state[bundle_index].start_time_received_counter = msg->message.start_time_proposal.step;
    if (msg->message.start_time_proposal.time > self->start_time_proposal) {
      self->start_time_proposal = msg->message.start_time_proposal.time;
    }
    break;
  case StartupCoordination_start_time_request_tag:
    if (self->state == STARTUP_COORDINATOR_STATE_DONE) {
      self->msg.which_message = FederateMessage_startup_coordination_tag;
      self->msg.message.startup_coordination.which_message = StartupCoordination_start_time_response_tag;
      self->msg.message.startup_coordination.message.start_time_response.time = self->start_time_proposal;
      self->env->net_bundles[bundle_index]->channel->send_blocking(self->env->net_bundles[bundle_index]->channel,
                                                                   &self->msg);
    }
    break;
  }
  self->env->leave_critical_section(self->env);
}

// FIXME: Fix this
void StartupCoordinator_ctor(StartupCoordinator *self, Environment *env, size_t longest_path) {
  self->env = env;
  self->longest_path = longest_path;
  self->state = STARTUP_COORDINATOR_STATE_UNINITIALIZED;
  for (int i = 0; i < num_neighbors; i++) {
    self->neighbor_state[i].handshake_received = false;
    self->neighbor_state[i].handshake_requested = false;
    self->neighbor_state[i].start_time_received_counter = 0;
  }
}