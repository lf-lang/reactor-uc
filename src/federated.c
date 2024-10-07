#include "reactor-uc/environment.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/platform.h"

// TODO: Refactor so this function is available
void LogicalConnection_trigger_downstreams(Connection *self, const void *value, size_t value_size);

// Called when a reaction does lf_set(outputPort)
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

  Connection_ctor(&self->super, TRIG_CONN_FEDERATED, bundle->parent, upstream, NULL, 0, NULL, NULL,
                  FederatedOutputConnection_cleanup, FederatedOutputConnection_trigger_downstream);
  self->staged = false;
  self->conn_id = conn_id;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
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
  if (self->trigger_value.staged) {
    validaten(true);
  }
}

// This should be called after receiving a PortMessage from the NetworkBundle and
// deserializing it.
void FederatedInputConnection_schedule(FederatedInputConnection *self, PortMessage *msg) {
  Environment *env = self->super.super.parent->env;
  Scheduler *sched = &env->scheduler;

  // TODO: Now we handle them as physical connections which doesnt need
  // a tag as part of the message.
  tag_t now_tag = {.time = env->get_physical_time(env), .microstep = 0};
  tag_t tag = lf_delay_tag(now_tag, self->delay);
  // FIXME: Is this thread-safe?
  self->trigger_value.stage(&self->trigger_value, &msg->message);
  self->trigger_value.push(&self->trigger_value);
  sched->schedule_at(sched, &self->super.super, tag);
}

// TODO: This assumes using one thread per Network bundle.
void FederatedConnectionBundle_thread(FederatedConnectionBundle *self) {
  validate(self);
  TcpIpBundle *bundle = self->net_bundle;
  PortMessage *msg = NULL;

  if (self->server) {
    // Bind to address
    bundle->bind(bundle);
  } else {
    // Connect to addreess
    bundle->connect(bundle);
  }

  // Use blocking API
  bundle->change_block_state(bundle, true);

  while (true) {
    msg = bundle->receive(bundle);
    validate(msg);
    validate((size_t)msg->connection_number < self->inputs_size);
    // Find the correct input port for this message
    FederatedInputConnection *input = self->inputs[msg->connection_number];
    input->schedule(input, msg);
  }
}