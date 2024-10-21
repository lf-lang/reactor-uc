#include "reactor-uc/environment.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/platform.h"

// TODO: Refactor so this function is available
void LogicalConnection_trigger_downstreams(Connection *self, const void *value, size_t value_size);

// Called when a reaction does lf_set(outputPort). Should buffer the output data
// for later transmission.
void FederatedOutputConnection_trigger_downstream(Connection *_self, const void *value, size_t value_size) {
  LF_DEBUG(FED, "Triggering downstreams on federated output connection %p. Stage for later TX", _self);
  FederatedOutputConnection *self = (FederatedOutputConnection *)_self;
  Scheduler *sched = &_self->super.parent->env->scheduler;
  validate(value);
  validate(value_size == self->value_size);
  memcpy(self->value_ptr, value, value_size);
  self->staged = true;
  sched->register_for_cleanup(sched, &_self->super);
}

// Called at the end of a logical tag if lf_set was called on the output
void FederatedOutputConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(FED, "Cleaning up federated output connection %p", trigger);
  FederatedOutputConnection *self = (FederatedOutputConnection *)trigger;
  Environment *env = trigger->parent->env;
  Scheduler *sched = &env->scheduler;
  NetworkChannel *channel = self->bundle->net_channel;
  validate(trigger->is_registered_for_cleanup);
  validaten(trigger->is_present);
  validate(self->staged);

  FederateMessage msg;
  msg.type = MessageType_TAGGED_MESSAGE;
  msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage *tagged_msg = &msg.message.tagged_message;
  tagged_msg->conn_id = self->conn_id;
  tagged_msg->tag.time = sched->current_tag.time;
  tagged_msg->tag.microstep = sched->current_tag.microstep;

  memcpy(tagged_msg->payload.bytes, self->value_ptr, self->value_size);
  tagged_msg->payload.size = self->value_size;

  LF_DEBUG(FED, "FedOutConn %p sending tagged message with tag=%" PRId64 ":%" PRIu32, trigger, tagged_msg->tag.time,
           tagged_msg->tag.microstep);
  lf_ret_t ret = channel->send(channel, &msg);

  if (ret != LF_OK) {
    LF_ERR(FED, "FedOutConn %p failed to send message", trigger);
  }
}

void FederatedOutputConnection_ctor(FederatedOutputConnection *self, Reactor *parent, FederatedConnectionBundle *bundle,
                                    int conn_id, void *value_ptr, size_t value_size) {

  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_OUTPUT, parent, NULL, 0, NULL, NULL,
                  FederatedOutputConnection_cleanup, FederatedOutputConnection_trigger_downstream);
  self->staged = false;
  self->conn_id = conn_id;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
  self->bundle = bundle;
}

// Called by Scheduler if an event for this Trigger is popped of event queue
void FederatedInputConnection_prepare(Trigger *trigger) {
  LF_DEBUG(FED, "Preparing federated input connection %p for triggering", trigger);
  FederatedInputConnection *self = (FederatedInputConnection *)trigger;
  Scheduler *sched = &trigger->parent->env->scheduler;
  TriggerDataQueue *tval = &self->trigger_data_queue;
  void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];
  trigger->is_present = true;

  sched->register_for_cleanup(sched, trigger);

  LogicalConnection_trigger_downstreams(&self->super, value_ptr, self->trigger_data_queue.value_size);
}

// Called at the end of a logical tag if it was registered for cleanup.
void FederatedInputConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(FED, "Cleaning up federated input connection %p", trigger);
  FederatedInputConnection *self = (FederatedInputConnection *)trigger;
  validate(trigger->is_registered_for_cleanup);

  if (trigger->is_present) {
    trigger->is_present = false;
    int ret = self->trigger_data_queue.pop(&self->trigger_data_queue);
    validaten(ret);
  }

  // Should never happen.
  validaten(self->trigger_data_queue.staged);
}

void FederatedInputConnection_ctor(FederatedInputConnection *self, Reactor *parent, interval_t delay, bool is_physical,
                                   Port **downstreams, size_t downstreams_size, void *value_buf, size_t value_size,
                                   size_t value_capacity) {
  TriggerDataQueue_ctor(&self->trigger_data_queue, value_buf, value_size, value_capacity);
  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_INPUT, parent, downstreams, downstreams_size,
                  &self->trigger_data_queue, FederatedInputConnection_prepare, FederatedInputConnection_cleanup, NULL);
  self->delay = delay;
  self->is_physical = is_physical;
  self->last_known_tag = NEVER_TAG;
  self->safe_to_assume_absent = FOREVER;
}

void FederatedConnectionBundle_handle_start_tag_signal(FederatedConnectionBundle *self, const FederateMessage *_msg) {
  const StartTagSignal *msg = &_msg->message.start_tag_signal;
  LF_DEBUG(FED, "Received start tag signal with tag=%" PRId64 ":%" PRIu32, msg->tag.time, msg->tag.microstep);
  Environment *env = self->parent->env;
  Scheduler *sched = &env->scheduler;
  env->platform->enter_critical_section(env->platform);

  if (sched->start_time == NEVER) {
    LF_DEBUG(FED, "First time receiving star tag. Setting tag and sending to other federates");
    sched->start_time = msg->tag.time;
    env->platform->new_async_event(env->platform);

    for (size_t i = 0; i < env->net_bundles_size; i++) {
      FederatedConnectionBundle *bundle = env->net_bundles[i];
      if (bundle != self) {
        bundle->net_channel->send(bundle->net_channel, _msg);
      }
    }
  } else {
    LF_DEBUG(FED, "Ignoring start tag signal. Already received one");
    assert(msg->tag.time == sched->start_time);
  }

  env->platform->leave_critical_section(env->platform);
}

// Callback registered with the NetworkChannel. Is called asynchronously when there is a
// a TaggedMessage available.
void FederatedConnectionBundle_handle_tagged_msg(FederatedConnectionBundle *self, const FederateMessage *_msg) {
  const TaggedMessage *msg = &_msg->message.tagged_message;
  LF_DEBUG(FED, "Callback on FedConnBundle %p for message with tag=%" PRId64 ":%" PRIu32, self, msg->tag.time,
           msg->tag.microstep);
  validate(((size_t)msg->conn_id) < self->inputs_size);
  FederatedInputConnection *input = self->inputs[msg->conn_id];
  Environment *env = self->parent->env;
  Scheduler *sched = &env->scheduler;
  env->platform->enter_critical_section(env->platform);

  // Calculate the tag at which we will schedule this event
  tag_t tag = {.time = msg->tag.time, .microstep = msg->tag.microstep};

  if (input->is_physical) {
    tag.time = env->get_physical_time(env);
    tag.microstep = 0;
  }
  tag = lf_delay_tag(tag, input->delay);
  LF_DEBUG(FED, "Scheduling input %p at tag=%" PRId64 ":%" PRIu32, input, tag.time, tag.microstep);

  // Take the value received over the network copy it into the trigger_data_queue of
  // the input port and schedule an event for it.
  input->trigger_data_queue.stage(&input->trigger_data_queue, &msg->payload.bytes);
  input->trigger_data_queue.push(&input->trigger_data_queue);
  lf_ret_t ret = sched->schedule_at_locked(sched, &input->super.super, tag);
  if (ret == LF_OK) {
    env->platform->new_async_event(env->platform);
  } else {
    LF_WARN(FED, "Failed to schedule input %p at tag=%" PRId64 ":%" PRIu32, input, tag.time, tag.microstep);
  }

  if (lf_tag_compare(input->last_known_tag, tag) < 0) {
    LF_DEBUG(FED, "Updating last known tag for input %p to %" PRId64 ":%" PRIu32, input, tag.time, tag.microstep);
    input->last_known_tag = tag;
  }

  env->platform->leave_critical_section(env->platform);
}

void FederatedConnectionBundle_msg_received_cb(FederatedConnectionBundle *self, const FederateMessage *msg) {
  switch (msg->type) {
  case MessageType_TAGGED_MESSAGE:
    FederatedConnectionBundle_handle_tagged_msg(self, msg);
    break;
  case MessageType_START_TAG_SIGNAL:
    FederatedConnectionBundle_handle_start_tag_signal(self, msg);
    break;
  case MessageType_REQUEST_ABSENT_SIGNAL:
    LF_ERR(FED, "Received request absent signal. Not implemented");
    break;
  case MessageType_ABSENT_SIGNAL:
    LF_ERR(FED, "Received absent signal. Not implemented");
    break;
  case MessageType_CONDITIONAL_ABSENT_SIGNAL:
    LF_ERR(FED, "Received conditional absent signal. Not implemented");
    break;
  }
}

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, NetworkChannel *net_channel,
                                    FederatedInputConnection **inputs, size_t inputs_size,
                                    FederatedOutputConnection **outputs, size_t outputs_size) {
  self->inputs = inputs;
  self->inputs_size = inputs_size;
  self->net_channel = net_channel;
  self->outputs = outputs;
  self->outputs_size = outputs_size;
  self->parent = parent;
  // Register callback function for message received.
  self->net_channel->register_callback(self->net_channel, FederatedConnectionBundle_msg_received_cb, self);
}
