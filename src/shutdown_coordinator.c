#include <reactor-uc/shutdown_coordinator.h>

#include "reactor-uc/environments/federated_environment.h"

#include <reactor-uc/logging.h>

#define NUM_RESERVED_EVENTS 3 // 3 events is reserved for scheduling our own events.

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
      LF_ERR(FED, "Failed to schedule shutdown system event.");
      validate(false);
    }
  } else {
    LF_ERR(FED, "Failed to allocate payload for incoming shutdown coordination message. Dropping it");
  }
}

static void ShutdownCoordinator_handle_time_proposal(ShutdownCoordinator* self, const ShutdownCoordination* msg,
                                                     size_t source_bundle_idx) {
  const tag_t announcement_tag = {.time = msg->message.shutdown_time_announcement.current_tag.time,
                                  .microstep = msg->message.shutdown_time_announcement.current_tag.microstep};
  const tag_t shutdown_tag = {.time = msg->message.shutdown_time_announcement.shutdown_tag.time,
                              .microstep = msg->message.shutdown_time_announcement.shutdown_tag.microstep};

  size_t current_step = msg->message.shutdown_time_announcement.step;
  bool shutdown_larger = lf_tag_compare(shutdown_tag, self->proposed_shutdown_time) == 1;
  bool announcement_time_equal = (bool)(lf_tag_compare(announcement_tag, self->announcement_of_shutdown) == 0 ||
                                 lf_tag_compare(self->announcement_of_shutdown, NEVER_TAG) == 0);

  if (shutdown_larger && announcement_time_equal && current_step <= self->longest_path) {
    LF_INFO(FED, "New and larger shutdown time received! Adjusting the shutdown time and broadcasting it.");
    self->proposed_shutdown_time = shutdown_tag;
    self->announcement_of_shutdown = announcement_tag;

    self->msg.which_message = FederateMessage_shutdown_coordination_tag;
    memcpy(&self->msg.message.shutdown_coordination, msg, sizeof(ShutdownCoordination));
    self->msg.message.shutdown_coordination.message.shutdown_time_announcement.step = current_step + 1;

    // now we forward this message with increased step count, to all neighbors
    for (size_t i = 0; i < ((FederatedEnvironment*)self->env)->net_bundles_size; i++) {
      if (source_bundle_idx != i) {
        const FederatedConnectionBundle* bundle = ((FederatedEnvironment*)self->env)->net_bundles[i];
        bundle->net_channel->send_blocking(bundle->net_channel, &self->msg);
      }
    }

    self->env->scheduler->request_shutdown(self->env->scheduler, self->proposed_shutdown_time);
  } else {
    LF_WARN(FED, "Shutdown message received but dropped! Current Step: %i", current_step);
  }
}

/** Invoked by scheduler when handling any system event destined for StartupCoordinator. */
static void ShutdownCoordinator_handle_system_event(SystemEventHandler* _self, SystemEvent* event) {
  ShutdownCoordinator* self = (ShutdownCoordinator*)_self;
  ShutdownEvent* payload = (ShutdownEvent*)event->super.payload;
  switch (payload->msg.which_message) {
  case ShutdownCoordination_shutdown_time_announcement_tag:
    LF_INFO(FED, "Handle: ShutdownCoordination time proposal tag");
    ShutdownCoordinator_handle_time_proposal(self, &payload->msg, payload->neighbor_index);
    break;
  default:
    throw("Unknown shutdown message, which cannot be handled!");
  }
}

void ShutdownCoordinator_shutdown(ShutdownCoordinator* self, interval_t shutdown_offset) {
  LF_INFO(FED, "User is requesting shutdown!");

  tag_t current_tag = self->env->scheduler->current_tag(self->env->scheduler);
  tag_t proposed_shutdown_time = lf_delay_tag(current_tag, shutdown_offset);

  // if (lf_tag_compare(self->announcement_of_shutdown, get_current_tag) != 1) {
  //   LF_WARN(FED, "Shutdown process already started!" PRINTF_TAG, self->announcement_of_shutdown);
  //   return;
  // }

  ShutdownEvent* payload = NULL;
  lf_ret_t ret = self->super.payload_pool.allocate(&self->super.payload_pool, (void**)&payload);

  if (ret == LF_OK) {
    payload->neighbor_index = -1;
    payload->msg.which_message = ShutdownCoordination_shutdown_time_announcement_tag;
    payload->msg.message.shutdown_time_announcement.step = 0;
    payload->msg.message.shutdown_time_announcement.current_tag.time = current_tag.time;
    payload->msg.message.shutdown_time_announcement.current_tag.microstep = current_tag.microstep;
    payload->msg.message.shutdown_time_announcement.shutdown_tag.time = proposed_shutdown_time.time;
    payload->msg.message.shutdown_time_announcement.shutdown_tag.microstep = proposed_shutdown_time.microstep;

    current_tag.microstep += 1;
    SystemEvent event = SYSTEM_EVENT_INIT(current_tag, &self->super, payload);
    event.handler->handle = ShutdownCoordinator_handle_system_event;

    self->env->scheduler->schedule_system_event_at(self->env->scheduler, &event);
  } else {
    LF_ERR(FED, "cannot shutdown, because buffer is full");
  }
}

void ShutdownCoordinator_ctor(ShutdownCoordinator* self, Environment* env, size_t longest_path, size_t payload_size,
                              void* payload_buf, bool* payload_used_buf, size_t payload_buf_capacity) {
  EventPayloadPool_ctor(&self->super.payload_pool, (char*)payload_buf, payload_used_buf, payload_size,
                        payload_buf_capacity, NUM_RESERVED_EVENTS);
  self->handle_message_callback = ShutdownCoordinator_handle_message_callback;
  self->shutdown = ShutdownCoordinator_shutdown;
  self->super.handle = ShutdownCoordinator_handle_system_event;
  self->env = env;
  self->announcement_of_shutdown = NEVER_TAG;
  self->proposed_shutdown_time = NEVER_TAG;
  self->longest_path = longest_path;
}
