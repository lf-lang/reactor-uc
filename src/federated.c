#include "reactor-uc/environment.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/platform.h"

void FederatedOutputConnection_trigger_downstream(Connection *_self, const void *value, size_t value_size) {
  FederatedOutputConnection *self = (FederatedOutputConnection *)_self;
  Scheduler *sched = &_self->super.parent->env->scheduler;
  validate(value);
  validate(value_size == self->value_size);
  memcpy(self->value_ptr, value, value_size);
  self->staged = true;
  sched->register_for_cleanup(sched, &_self->super);
}
void FederatedOutputConnection_cleanup(Trigger *trigger) {
  FederatedOutputConnection *self = (FederatedOutputConnection *)trigger;
  TcpIpBundle *bundle = self->bundle;
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

void FederatedInputConnection_prepare(Trigger *trigger) {
  FederatedInputConnection *self = (FederatedInputConnection *)trigger;
  Scheduler *sched = &trigger->parent->env->scheduler;
  TriggerValue *tval = &self->trigger_value;
  void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];
  trigger->is_present = true;

  sched->register_for_cleanup(sched, trigger);

  LogicalConnection_trigger_downstreams(&self->super, value_ptr, self->trigger_value.value_size);
}

void FederatedInputConnection_cleanup(Trigger *trigger) {
  FederatedInputConnection *self = (FederatedInputConnection *)trigger;
  validate(trigger->is_registered_for_cleanup);

  if (trigger->is_present) {
    trigger->is_present = false;
    int ret = self->trigger_value.pop(&self->trigger_value);
    validaten(ret);
  }

  if (self->trigger_value.staged) {
    validaten(true);
  }
}

void FederatedInputConnection_schedule(FederatedInputConnection *self, PortMessage *msg) {

  Environment *env = self->super.super.parent->env;
  Scheduler *sched = &env->scheduler;

  // TODO: Now we handle them as physical connections which doesnt need
  // a tag as part of the message.
  tag_t now_tag = {.time = env->get_physical_time(env), .microstep = 0};
  tag_t tag = lf_delay_tag(now_tag, self->delay);
  self->trigger_value.stage(msg->message);
  self->trigger_value.push(&self->trigger_value);
  sched->schedule_at(sched, &self->super.super, tag);
}
