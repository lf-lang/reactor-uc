#include "reactor-uc/federated.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/platform.h"

// TODO: Refactor so this function is available
void LogicalConnection_trigger_downstreams(Connection *self, const void *value, size_t value_size);

// Called when a reaction does lf_set(outputPort). Should buffer the output data
// for later transmission.
void FederatedOutputConnection_trigger_downstream(Connection *_self, const void *value, size_t value_size) {
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
  FederatedOutputConnection *self = (FederatedOutputConnection *)trigger;
  Environment *env = trigger->parent->env;
  NetworkBundle *bundle = self->bundle->net_bundle;
  validate(trigger->is_registered_for_cleanup);
  validaten(trigger->is_present);
  validate(self->staged);

  TaggedMessage msg;
  msg.conn_id = self->conn_id;

  msg.tag.time = env->current_tag.time;
  msg.tag.microstep = env->current_tag.microstep;

  memcpy(msg.payload.bytes, self->value_ptr, self->value_size);
  msg.payload.size = self->value_size;

  int resp = bundle->send(bundle, &msg);

  // TODO: Do error handling.
  validate(resp == LF_OK);
}

void FederatedOutputConnection_ctor(FederatedOutputConnection *self, Reactor *parent, FederatedConnectionBundle *bundle,
                                    int conn_id, Port *upstream, void *value_ptr, size_t value_size) {

  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_OUTPUT, parent, upstream, NULL, 0, NULL, NULL,
                  FederatedOutputConnection_cleanup, FederatedOutputConnection_trigger_downstream);
  self->staged = false;
  self->conn_id = conn_id;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
  self->bundle = bundle;
}

// Called by Scheduler if an event for this Trigger is popped of event queue
void FederatedInputConnection_prepare(Trigger *trigger) {
  FederatedInputConnection *self = (FederatedInputConnection *)trigger;
  Scheduler *sched = &trigger->parent->env->scheduler;
  TriggerValue *tval = &self->trigger_value;
  void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];
  trigger->is_present = true;

  sched->register_for_cleanup(sched, trigger);

  LogicalConnection_trigger_downstreams(&self->super, value_ptr, self->trigger_value.value_size);
}

// Called at the end of a logical tag if it was registered for cleanup.
void FederatedInputConnection_cleanup(Trigger *trigger) {
  FederatedInputConnection *self = (FederatedInputConnection *)trigger;
  validate(trigger->is_registered_for_cleanup);

  if (trigger->is_present) {
    trigger->is_present = false;
    int ret = self->trigger_value.pop(&self->trigger_value);
    validaten(ret);
  }

  // Should never happen.
  validaten(self->trigger_value.staged);
}

void FederatedInputConnection_ctor(FederatedInputConnection *self, Reactor *parent, interval_t delay, bool is_physical,
                                   Port **downstreams, size_t downstreams_size, void *value_buf, size_t value_size,
                                   size_t value_capacity) {
  TriggerValue_ctor(&self->trigger_value, value_buf, value_size, value_capacity);
  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_INPUT, parent, NULL, downstreams, downstreams_size,
                  &self->trigger_value, FederatedInputConnection_prepare, FederatedInputConnection_cleanup, NULL);
  self->delay = delay;
  self->is_physical = is_physical;
  self->last_known_tag = NEVER_TAG;
  self->safe_to_assume_absent = FOREVER;
}

// Callback registered with the NetworkBundle. Is called asynchronously when there is a
// a TaggedMessage available.
void FederatedConnectionBundle_msg_received_cb(FederatedConnectionBundle *self, TaggedMessage *msg) {
  validate(((size_t)msg->conn_id) < self->inputs_size);
  FederatedInputConnection *input = self->inputs[msg->conn_id];
  Environment *env = self->parent->env;
  Scheduler *sched = &env->scheduler;
  env->platform->enter_critical_section(env->platform);

  // Calculate the tag at which we will schedule this event
  tag_t tag = {.time = msg->tag.time, .microstep = msg->tag.microstep};

  printf("Received message with tag=%ld\n", msg->tag.time - env->start_time);
  if (input->is_physical) {
    tag.time = env->get_physical_time(env);
    tag.microstep = 0;
  }
  tag = lf_delay_tag(tag, input->delay);
  printf("Scheduling at tag=%ld\n", tag.time - env->start_time);

  // Take the value received over the network copy it into the trigger_value of
  // the input port and schedule an event for it.
  input->trigger_value.stage(&input->trigger_value, &msg->payload.bytes);
  input->trigger_value.push(&input->trigger_value);
  lf_ret_t ret = sched->schedule_at_locked(sched, &input->super.super, tag);
  if (ret == LF_OK) {
    env->platform->new_async_event(env->platform);
  }

  if (lf_tag_compare(input->last_known_tag, tag) < 0) {
    input->last_known_tag = tag;
  }

  env->platform->leave_critical_section(env->platform);
}

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, NetworkBundle *net_bundle,
                                    FederatedInputConnection **inputs, size_t inputs_size,
                                    FederatedOutputConnection **outputs, size_t outputs_size) {
  self->inputs = inputs;
  self->inputs_size = inputs_size;
  self->net_bundle = net_bundle;
  self->outputs = outputs;
  self->outputs_size = outputs_size;
  self->parent = parent;
  // Register callback function for message received.
  self->net_bundle->register_callback(self->net_bundle, FederatedConnectionBundle_msg_received_cb, self);
}
