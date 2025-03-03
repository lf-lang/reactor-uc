#include "reactor-uc/federated.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/tag.h"

// Called when a reaction does lf_set(outputPort). Should buffer the output data
// for later transmission.
void FederatedOutputConnection_trigger_downstream(Connection *_self, const void *value, size_t value_size) {
  LF_DEBUG(FED, "Triggering downstreams on federated output connection %p. Stage for later TX", _self);
  lf_ret_t ret;
  FederatedOutputConnection *self = (FederatedOutputConnection *)_self;
  Scheduler *sched = _self->super.parent->env->scheduler;
  Trigger *trigger = &_self->super;
  EventPayloadPool *pool = trigger->payload_pool;

  assert(value);
  assert(value_size == self->payload_pool.payload_size);

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
  lf_ret_t ret;
  FederatedOutputConnection *self = (FederatedOutputConnection *)trigger;
  Environment *env = trigger->parent->env;
  Scheduler *sched = env->scheduler;
  NetworkChannel *channel = self->bundle->net_channel;

  EventPayloadPool *pool = trigger->payload_pool;

  if (channel->is_connected(channel)) {
    assert(self->staged_payload_ptr);
    assert(trigger->is_registered_for_cleanup);
    assert(trigger->is_present == false);

    self->bundle->send_msg.which_message = FederateMessage_tagged_message_tag;

    TaggedMessage *tagged_msg = &self->bundle->send_msg.message.tagged_message;
    tagged_msg->conn_id = self->conn_id;
    tagged_msg->tag.time = sched->current_tag(sched).time;
    tagged_msg->tag.microstep = sched->current_tag(sched).microstep;

    assert(self->bundle->serialize_hooks[self->conn_id]);
    int msg_size = (*self->bundle->serialize_hooks[self->conn_id])(
        self->staged_payload_ptr, self->payload_pool.payload_size, tagged_msg->payload.bytes);
    if (msg_size < 0) {
      LF_ERR(FED, "Failed to serialize payload for federated output connection %p", trigger);
    } else {
      tagged_msg->payload.size = msg_size;

      LF_DEBUG(FED, "FedOutConn %p sending tagged message with tag:" PRINTF_TAG, trigger, tagged_msg->tag);
      if (channel->send_blocking(channel, &self->bundle->send_msg) != LF_OK) {
        LF_ERR(FED, "FedOutConn %p failed to send message", trigger);
      }
    }
  } else {
    LF_WARN(FED, "FedOutConn %p not connected. Dropping staged message", trigger);
  }

  ret = pool->free(pool, self->staged_payload_ptr);
  if (ret != LF_OK) {
    LF_ERR(FED, "FedOutConn %p failed to free staged payload", trigger);
  }
  self->staged_payload_ptr = NULL;
  LF_DEBUG(FED, "Federated output connection %p cleaned up", trigger);
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
  Scheduler *sched = trigger->parent->env->scheduler;
  EventPayloadPool *pool = trigger->payload_pool;
  trigger->is_present = true;
  sched->register_for_cleanup(sched, trigger);

  assert(self->super.downstreams_size == 1);
  Port *down = self->super.downstreams[0];

  if (down->effects.size > 0 || down->observers.size > 0) {
    validate(pool->payload_size == down->value_size);
    memcpy(down->value_ptr, event->super.payload, pool->payload_size); // NOLINT
    down->super.prepare(&down->super, event);
  }

  for (size_t i = 0; i < down->conns_out_registered; i++) {
    LF_DEBUG(CONN, "Found further downstream connection %p to recurse down", down->conns_out[i]);
    down->conns_out[i]->trigger_downstreams(down->conns_out[i], event->super.payload, pool->payload_size);
  }
  pool->free(pool, event->super.payload);
}

// Called at the end of a logical tag if it was registered for cleanup.
void FederatedInputConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(FED, "Cleaning up federated input connection %p", trigger);
  assert(trigger->is_registered_for_cleanup);
  assert(trigger->is_present);
  trigger->is_present = false;
}

void FederatedInputConnection_ctor(FederatedInputConnection *self, Reactor *parent, interval_t delay, bool is_physical,
                                   interval_t max_wait, Port **downstreams, size_t downstreams_size, void *payload_buf,
                                   bool *payload_used_buf, size_t payload_size, size_t payload_buf_capacity) {
  EventPayloadPool_ctor(&self->payload_pool, payload_buf, payload_used_buf, payload_size, payload_buf_capacity);
  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_INPUT, parent, downstreams, downstreams_size, &self->payload_pool,
                  FederatedInputConnection_prepare, FederatedInputConnection_cleanup, NULL);
  self->delay = delay;
  self->type = is_physical ? PHYSICAL_CONNECTION : LOGICAL_CONNECTION;
  self->last_known_tag = NEVER_TAG;
  self->max_wait = max_wait;
}

// Callback registered with the NetworkChannel. Is called asynchronously when there is a
// a TaggedMessage available.
void FederatedConnectionBundle_handle_tagged_msg(FederatedConnectionBundle *self, const FederateMessage *_msg) {
  const TaggedMessage *msg = &_msg->message.tagged_message;
  LF_DEBUG(FED, "Callback on FedConnBundle %p for message of size=%u with tag:" PRINTF_TAG, self, msg->payload.size,
           msg->tag);
  assert(((size_t)msg->conn_id) < self->inputs_size);
  lf_ret_t ret;
  FederatedInputConnection *input = self->inputs[msg->conn_id];
  Environment *env = self->parent->env;
  Scheduler *sched = env->scheduler;
  EventPayloadPool *pool = &input->payload_pool;

  // Verify that we have started executing and can actually handle it
  if (sched->start_time == NEVER) {
    LF_ERR(FED, "Received message before start tag. Dropping");
    return;
  }

  tag_t base_tag = ZERO_TAG;
  if (input->type == PHYSICAL_CONNECTION) {
    base_tag.time = env->get_physical_time(env);
  } else {
    base_tag.time = msg->tag.time;
    base_tag.microstep = msg->tag.microstep;
  }

  tag_t tag = lf_delay_tag(base_tag, input->delay);
  LF_DEBUG(FED, "Scheduling input %p at tag: " PRINTF_TAG, input, tag);

  // Take the value received over the network copy it into the payload_pool of
  // the input port and schedule an event for it.
  void *payload;
  ret = pool->allocate(pool, &payload);
  if (ret != LF_OK) {
    LF_ERR(FED, "Input buffer at Connection %p is full. Dropping incoming msg", input);
  } else {
    lf_ret_t status = (*self->deserialize_hooks[msg->conn_id])(payload, msg->payload.bytes, msg->payload.size);

    if (status == LF_OK) {
      Event event = EVENT_INIT(tag, &input->super.super, payload);
      ret = sched->schedule_at_locked(sched, &event.super);
      switch (ret) {
      case LF_AFTER_STOP_TAG:
        LF_WARN(FED, "Tried scheduling event after stop tag. Dropping");
        break;
      case LF_PAST_TAG:
        LF_INFO(FED, "Safe-to-process violation! Tried scheduling event to a past tag. Handling now instead!");
        event.super.tag = sched->current_tag(sched);
        event.super.tag.microstep++;
        status = sched->schedule_at_locked(sched, &event.super);
        if (status != LF_OK) {
          LF_ERR(FED, "Failed to schedule event at current tag also. Dropping");
        }
        break;
      case LF_INVALID_TAG:
        LF_WARN(FED, "Dropping event with invalid tag");
        break;
      case LF_OK:
        break;
      default:
        LF_ERR(FED, "Unknown return value `%d` from schedule_at_locked", ret);
        validate(false);
        break;
      }
    } else {
      LF_ERR(FED, "Cannot deserialize message from other Federate. Dropping");
    }

    if (lf_tag_compare(input->last_known_tag, tag) < 0) {
      LF_DEBUG(FED, "Updating last known tag for input %p to " PRINTF_TAG, input, tag);
      input->last_known_tag = tag;
    }
  }
}

void FederatedConnectionBundle_msg_received_cb(FederatedConnectionBundle *self, const FederateMessage *msg) {
  // This function is invoked asynchronously from the network channel. We must thus enter a critical
  // section before we do anything.
  self->parent->env->enter_critical_section(self->parent->env);
  switch (msg->which_message) {
  case FederateMessage_tagged_message_tag:
    FederatedConnectionBundle_handle_tagged_msg(self, msg);
    break;
  case FederateMessage_startup_coordination_tag:
    self->parent->env->startup_coordinator->handle_message_callback(self->parent->env->startup_coordinator,
                                                                    &msg->message.startup_coordination, self->index);
    break;
  case FederateMessage_clock_sync_msg_tag:
    if (self->parent->env->do_clock_sync) {
      self->parent->env->clock_sync->handle_message_callback(self->parent->env->clock_sync,
                                                             &msg->message.clock_sync_msg, self->index);
    } else {
      LF_WARN(FED, "Received clock-sync message but clock-sync is disabled. Ignoring");
    }

    break;
  default:
    LF_ERR(FED, "Unknown message type %d", msg->which_message);
    assert(false);
  }
  // Leave critical section before returning back to the network channel.
  self->parent->env->leave_critical_section(self->parent->env);
}

void FederatedConnectionBundle_ctor(FederatedConnectionBundle *self, Reactor *parent, NetworkChannel *net_channel,
                                    FederatedInputConnection **inputs, deserialize_hook *deserialize_hooks,
                                    size_t inputs_size, FederatedOutputConnection **outputs,
                                    serialize_hook *serialize_hooks, size_t outputs_size, size_t index) {
  validate(self);
  validate(parent);
  validate(net_channel);
  self->inputs = inputs;
  self->inputs_size = inputs_size;
  self->net_channel = net_channel;
  self->outputs = outputs;
  self->outputs_size = outputs_size;
  self->parent = parent;
  self->deserialize_hooks = deserialize_hooks;
  self->serialize_hooks = serialize_hooks;
  self->index = index;
  self->net_channel->register_receive_callback(self->net_channel, FederatedConnectionBundle_msg_received_cb, self);
}

void FederatedConnectionBundle_validate(FederatedConnectionBundle *bundle) {
  validate(bundle);
  validate(bundle->net_channel);
  for (size_t i = 0; i < bundle->inputs_size; i++) {
    validate(bundle->inputs[i]);
    validate(bundle->deserialize_hooks[i]);
    validate(bundle->inputs[i]->super.super.parent);
    validate(bundle->inputs[i]->super.super.payload_pool->payload_size < SERIALIZATION_MAX_PAYLOAD_SIZE);
  }
  for (size_t i = 0; i < bundle->outputs_size; i++) {
    validate(bundle->outputs[i]);
    validate(bundle->serialize_hooks[i]);
    validate(bundle->outputs[i]->super.super.parent);
    validate(bundle->outputs[i]->super.super.payload_pool->payload_size < SERIALIZATION_MAX_PAYLOAD_SIZE);
  }
}
