#include <reactor-uc/shutdown_coordinator.h>

#include "reactor-uc/environments/federated_environment.h"

#include <reactor-uc/logging.h>

/** Handle an incoming message from the network. Invoked from an async context in a critical section. */
static void ShutdownCoordinator_handle_message_callback(ShutdownCoordinator* self, const ShutdownCoordination* msg,
                                                       size_t bundle_idx) {
  LF_DEBUG(FED, "Received startup message from neighbor %zu. Scheduling as a system event", bundle_idx);
  ShutdownEvent* payload = NULL;
  lf_ret_t ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void**)&payload);
  if (ret == LF_OK) {
    payload->neighbor_index = bundle_idx;
    memcpy(&payload->msg, msg, sizeof(ShutdownCoordination));
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

static void ShutdownCoordinator_handle_time_proposal(ShutdownCoordinator* self, const ShutdownCoordination* msg, size_t source_bundle_idx) {
  if (msg->message.shutdown_time_proposal.time > self->proposed_shutdown_time && msg->message.shutdown_time_proposal.step < self->longest_path) {
    LF_INFO(FED, "New and larger shutdown time received! Adjusting the shutdown time and broadcasting it.");
    self->proposed_shutdown_time = msg->message.shutdown_time_proposal.time;

    memcpy(&self->msg, msg, sizeof(ShutdownCoordination));
    self->msg.message.shutdown_coordination.message.shutdown_time_proposal.step = msg->message.shutdown_time_proposal.step + 1;
    for (size_t i = 0; i < ((FederatedEnvironment*)self->env)->net_bundles_size; i++) {
      if (source_bundle_idx != i) {
        FederatedConnectionBundle* bundle = ((FederatedEnvironment*)self->env)->net_bundles[i];
        bundle->net_channel->send_blocking(bundle->net_channel, &self->msg);
      }
    }

    self->env->scheduler->request_shutdown(self->env->scheduler, self->proposed_shutdown_time);
  }
}


/** Invoked by scheduler when handling any system event destined for StartupCoordinator. */
static void ShutdownCoordinator_handle_system_event(SystemEventHandler* _self, SystemEvent* event) {
  ShutdownCoordinator* self = (ShutdownCoordinator*)_self;
  ShutdownEvent* payload = (ShutdownEvent*)event->super.payload;
  switch (payload->msg.which_message) {
  case ShutdownCoordination_shutdown_time_proposal_tag:
    LF_INFO(FED, "Handle: ShutdownCoordination time proposal tag");
    ShutdownCoordinator_handle_time_proposal(self, &payload->msg, payload->neighbor_index);
    break;
  }
}


void ShutdownCoordinator_ctor(ShutdownCoordinator* self) {
  self->handle_message_callback = ShutdownCoordinator_handle_message_callback;
  self->super.handle = ShutdownCoordinator_handle_system_event;
}