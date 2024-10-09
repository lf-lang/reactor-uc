#include "reactor-uc/environment.h"
#include "reactor-uc/federated.h"
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
  TcpIpBundle *bundle = self->bundle->net_bundle;
  validate(trigger->is_registered_for_cleanup);
  validaten(trigger->is_present);
  validate(self->staged);

  PortMessage msg;
  msg.connection_number = self->conn_id;
  memcpy(msg.message, self->value_ptr, self->value_size);
  int resp = bundle->send(bundle, &msg);

  // TODO: Do error handling.
  validate(resp == SUCCESS);
}

void FederatedOutputConnection_ctor(FederatedOutputConnection *self, FederatedConnectionBundle *bundle, int conn_id,
                                    Port *upstream, void *value_ptr, size_t value_size) {

  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_OUTPUT, bundle->parent, upstream, NULL, 0, NULL, NULL,
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
}

// Callback registered with the NetworkBundle. Is called asynchronously when there is a
// a PortMessage available.
void FederatedConnectionBundle_msg_received_cb(FederatedConnectionBundle *self, PortMessage *msg) {
  validate(((size_t)msg->connection_number) < self->inputs_size);
  FederatedInputConnection *input = self->inputs[msg->connection_number];
  Environment *env = self->parent->env;
  Scheduler *sched = &env->scheduler;

  // TODO: Now we handle them as physical connections which doesnt need
  // a tag as part of the message.
  tag_t now_tag = {.time = env->get_physical_time(env), .microstep = 0};
  tag_t tag = lf_delay_tag(now_tag, input->delay);
  // FIXME: Is this thread-safe?
  input->trigger_value.stage(&input->trigger_value, &msg->message);
  input->trigger_value.push(&input->trigger_value);
  sched->schedule_at(sched, &input->super.super, tag);
}

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, TcpIpBundle *net_bundle,
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
