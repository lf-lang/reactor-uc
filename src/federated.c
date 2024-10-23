#include "reactor-uc/federated.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/platform.h"

// TODO: Refactor so this function is available
void LogicalConnection_trigger_downstreams(Connection *self, const void *value, size_t value_size);

// Called when a reaction does lf_set(outputPort). Should buffer the output data
// for later transmission.
void FederatedOutputConnection_trigger_downstream(Connection *_self, const void *value, size_t value_size) {
  LF_DEBUG(FED, "Triggering downstreams on federated output connection %p. Stage for later TX", _self);
  lf_ret_t ret;
  FederatedOutputConnection *self = (FederatedOutputConnection *)_self;
  Scheduler *sched = &_self->super.parent->env->scheduler;
  Trigger *trigger = &_self->super;
  EventPayloadPool *pool = trigger->payload_pool;

  assert(value);
  assert(value_size == self->payload_pool.size);

  if (self->staged_payload_ptr == NULL) {
    ret = pool->allocate(pool, &self->staged_payload_ptr);
    if (ret != LF_OK) {
      LF_ERR(FED, "Output buffer in Connection %p is full", _self);
      return;
    }
  }

  memcpy(self->staged_payload_ptr, value, value_size);
  sched->register_for_cleanup(sched, &_self->super);
}

// Called at the end of a logical tag if lf_set was called on the output
void FederatedOutputConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(FED, "Cleaning up federated output connection %p", trigger);
  FederatedOutputConnection *self = (FederatedOutputConnection *)trigger;
  Environment *env = trigger->parent->env;
  Scheduler *sched = &env->scheduler;
  NetworkChannel *channel = self->bundle->net_channel;
  EventPayloadPool *pool = trigger->payload_pool;
  assert(self->staged_payload_ptr);
  assert(trigger->is_registered_for_cleanup);
  assert(trigger->is_present == false);

  TaggedMessage msg;
  msg.conn_id = self->conn_id;
  msg.tag.time = sched->current_tag.time;
  msg.tag.microstep = sched->current_tag.microstep;

  memcpy(msg.payload.bytes, self->staged_payload_ptr, self->payload_pool.size);
  msg.payload.size = self->payload_pool.size;

  LF_DEBUG(FED, "FedOutConn %p sending message with tag=%" PRId64 ":%" PRIu32, trigger, msg.tag.time,
           msg.tag.microstep);
  lf_ret_t ret = channel->send(channel, &msg);

  if (ret != LF_OK) {
    LF_ERR(FED, "FedOutConn %p failed to send message", trigger);
  }
  ret = pool->free(pool, self->staged_payload_ptr);
  if (ret != LF_OK) {
    LF_ERR(FED, "FedOutConn %p failed to free staged payload", trigger);
  }
  self->staged_payload_ptr = NULL;
}

void FederatedOutputConnection_ctor(FederatedOutputConnection *self, Reactor *parent, FederatedConnectionBundle *bundle,
                                    int conn_id, void *payload_buf, bool *payload_used_buf, size_t payload_size,
                                    size_t payload_buf_capacity) {

  EventPayloadPool_ctor(&self->payload_pool, payload_buf, payload_used_buf, payload_size, payload_buf_capacity);
  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_OUTPUT, parent, NULL, payload_size, &self->payload_pool, NULL,
                  FederatedOutputConnection_cleanup, FederatedOutputConnection_trigger_downstream);
  self->conn_id = conn_id;
  self->bundle = bundle;
}

// Called by Scheduler if an event for this Trigger is popped of event queue
void FederatedInputConnection_prepare(Trigger *trigger, Event *event) {
  (void)event;
  LF_DEBUG(FED, "Preparing federated input connection %p for triggering", trigger);
  FederatedInputConnection *self = (FederatedInputConnection *)trigger;
  Scheduler *sched = &trigger->parent->env->scheduler;
  EventPayloadPool *pool = trigger->payload_pool;
  trigger->is_present = true;
  sched->register_for_cleanup(sched, trigger);

  LogicalConnection_trigger_downstreams(&self->super, event->payload, pool->size);
  pool->free(pool, event->payload);
}

// Called at the end of a logical tag if it was registered for cleanup.
void FederatedInputConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(FED, "Cleaning up federated input connection %p", trigger);
  assert(trigger->is_registered_for_cleanup);
  assert(trigger->is_present);
  trigger->is_present = false;
}

void FederatedInputConnection_ctor(FederatedInputConnection *self, Reactor *parent, interval_t delay,
                                   ConnectionType type, Port **downstreams, size_t downstreams_size, void *payload_buf,
                                   bool *payload_used_buf, size_t payload_size, size_t payload_buf_capacity) {
  EventPayloadPool_ctor(&self->payload_pool, payload_buf, payload_used_buf, payload_size, payload_buf_capacity);
  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_INPUT, parent, downstreams, downstreams_size, &self->payload_pool,
                  FederatedInputConnection_prepare, FederatedInputConnection_cleanup, NULL);
  self->delay = delay;
  self->type = type;
  self->last_known_tag = NEVER_TAG;
  self->safe_to_assume_absent = FOREVER;
}

// Callback registered with the NetworkChannel. Is called asynchronously when there is a
// a TaggedMessage available.
void FederatedConnectionBundle_msg_received_cb(FederatedConnectionBundle *self, TaggedMessage *msg) {
  LF_DEBUG(FED, "Callback on FedConnBundle %p for message with tag=%" PRId64 ":%" PRIu32, self, msg->tag.time,
           msg->tag.microstep);
  assert(((size_t)msg->conn_id) < self->inputs_size);
  lf_ret_t ret;
  FederatedInputConnection *input = self->inputs[msg->conn_id];
  Environment *env = self->parent->env;
  Scheduler *sched = &env->scheduler;
  EventPayloadPool *pool = &input->payload_pool;
  env->platform->enter_critical_section(env->platform);

  tag_t base_tag = ZERO_TAG;
  if (input->type == PHYSICAL_CONNECTION) {
    base_tag.time = env->get_physical_time(env);
  } else {
    base_tag.time = msg->tag.time;
    base_tag.microstep = msg->tag.microstep;
  }

  tag_t tag = lf_delay_tag(base_tag, input->delay);
  LF_DEBUG(FED, "Scheduling input %p at tag=%" PRId64 ":%" PRIu32, input, tag.time, tag.microstep);

  // Take the value received over the network copy it into the payload_pool of
  // the input port and schedule an event for it.
  void *payload;
  ret = pool->allocate(pool, &payload);
  if (ret != LF_OK) {
    LF_ERR(FED, "Input buffer at Connection %p is full. Dropping incoming msg", input);
  } else {
    memcpy(payload, msg->payload.bytes, msg->payload.size);
    Event event = EVENT_INIT(tag, &input->super.super, payload);
    ret = sched->schedule_at_locked(sched, &event);
    switch (ret) {
    case LF_AFTER_STOP_TAG:
      LF_WARN(FED, "Tried scheduling event after stop tag. Dropping\n");
      break;
    case LF_PAST_TAG:
      LF_WARN(FED, "Tried scheduling event to a past tag. Dropping\n");
      break;
    case LF_OK:
      env->platform->new_async_event(env->platform);
      break;
    default:
      LF_ERR(FED, "Unknown return value `%d` from schedule_at_locked\n", ret);
      validate(false);
      break;
    }

    if (lf_tag_compare(input->last_known_tag, tag) < 0) {
      LF_DEBUG(FED, "Updating last known tag for input %p to %" PRId64 ":%" PRIu32, input, tag.time, tag.microstep);
      input->last_known_tag = tag;
    }
  }

  env->platform->leave_critical_section(env->platform);
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
