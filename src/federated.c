#include "reactor-uc/federated.h"
#include <stddef.h>
#include <limits.h>
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/tag.h"

#define FEDERATED_OUTPUT_LEVEL_UNKNOWN INT_MIN

// Executed by the scheduler once all reactions that can write the upstream port have been executed at the
// current tag. Sends the staged value (i.e. the value of the last write) to the downstream federate.
static void FederatedOutputConnection_flush(LevelDeferredAction* action) {
  FederatedOutputConnection* self =
      (FederatedOutputConnection*)((char*)action - offsetof(FederatedOutputConnection, flush)); // NOLINT
  Trigger* trigger = &self->super.super;
  Scheduler* sched = trigger->parent->env->scheduler;
  NetworkChannel* channel = self->bundle->net_channel;
  LF_INFO(FED, "Flushing federated output connection %p", trigger);

  if (channel->is_connected(channel)) {
    self->bundle->send_msg.which_message = FederateMessage_tagged_message_tag;

    TaggedMessage* tagged_msg = &self->bundle->send_msg.message.tagged_message;
    tagged_msg->conn_id = self->conn_id;
    tagged_msg->tag.time = sched->current_tag(sched).time;
    tagged_msg->tag.microstep = sched->current_tag(sched).microstep;

    int msg_size = 0;
    if (self->payload_size > 0) {
      assert(self->bundle->serialize_hooks[self->conn_id]);
      msg_size = (*self->bundle->serialize_hooks[self->conn_id])(self->payload_buf, self->payload_size,
                                                                 tagged_msg->payload.bytes);
    }

    if (msg_size < 0) {
      LF_ERR(FED, "Failed to serialize payload for federated output connection %p", trigger);
    } else {
      tagged_msg->payload.size = msg_size;

      LF_DEBUG(FED, "FedOutConn %p sending tagged message conn_id=%d size=%u tag:" PRINTF_TAG, trigger,
               tagged_msg->conn_id, tagged_msg->payload.size, tagged_msg->tag);
      if (channel->send_blocking(channel, &self->bundle->send_msg) != LF_OK) {
        LF_ERR(FED, "FedOutConn %p failed to send message", trigger);
      }
    }
  } else {
    LF_WARN(FED, "FedOutConn %p not connected. Dropping staged message", trigger);
  }
}

// Called when a reaction does lf_set(outputPort). Stages the value and defers sending it until all
// reactions that can write the port have been executed at the current tag ("last write wins").
void FederatedOutputConnection_trigger_downstream(Connection* _self, tag_t intended_tag, const void* value,
                                                  size_t value_size) {
  (void)intended_tag; // The message is sent with the current tag.
  LF_DEBUG(FED, "Triggering downstreams on federated output connection %p. Stage for later TX", _self);
  FederatedOutputConnection* self = (FederatedOutputConnection*)_self;
  Scheduler* sched = _self->super.parent->env->scheduler;

  validate(value_size == self->payload_size);
  if (value_size > 0) {
    memcpy(self->payload_buf, value, value_size); // NOLINT
  }

  if (!self->flush.pending) {
    if (self->flush.level == FEDERATED_OUTPUT_LEVEL_UNKNOWN) {
      // The levels of all reactions are known once the program is running.
      self->flush.level = calculate_port_level(_self->upstream);
    }
    sched->defer_until_level_done(sched, &self->flush);
  }
}

void FederatedOutputConnection_ctor(FederatedOutputConnection* self, Reactor* parent, FederatedConnectionBundle* bundle,
                                    int conn_id, void* payload_buf, size_t payload_size) {
  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_OUTPUT, parent, NULL, 0, NULL, NULL, NULL,
                  FederatedOutputConnection_trigger_downstream);
  self->conn_id = conn_id;
  self->bundle = bundle;
  self->payload_buf = payload_buf;
  self->payload_size = payload_size;
  self->flush.next = NULL;
  self->flush.level = FEDERATED_OUTPUT_LEVEL_UNKNOWN;
  self->flush.pending = false;
  self->flush.execute = FederatedOutputConnection_flush;
}

// Called by Scheduler if an event for this Trigger is popped of event queue
void FederatedInputConnection_prepare(Trigger* trigger, Event* event) {
  (void)event;
  LF_DEBUG(FED, "Preparing federated input connection %p for triggering", trigger);
  FederatedInputConnection* self = (FederatedInputConnection*)trigger;
  Environment* env = trigger->parent->env;
  Scheduler* sched = env->scheduler;
  EventPayloadPool* pool = trigger->payload_pool;
  trigger->is_present = true;
  sched->register_for_cleanup(sched, trigger);

  assert(self->super.downstreams_size == 1);
  Port* down = self->super.downstreams[0];

  if (down->effects.size > 0 || down->observers.size > 0) {
    validate(pool->payload_size == down->value_size);
    if (down->value_size > 0) {
      memcpy(down->value_ptr, event->super.payload, pool->payload_size); // NOLINT
    }
    LF_INFO(FED, "FederatedInputConnection %p preparing downstream port %p for tag: " PRINTF_TAG, (void*)trigger,
            (void*)down, event->super.tag.time, event->super.tag.microstep);
    if (pool->payload_size >= sizeof(int)) {
      int payload_int = 0;
      memcpy(&payload_int, event->super.payload, sizeof(int));
      LF_INFO(FED, "  payload first int = %d", payload_int);
    }
    down->super.prepare(&down->super, event);
  }

  for (size_t i = 0; i < down->conns_out_registered; i++) {
    LF_DEBUG(CONN, "Found further downstream connection %p to recurse down", down->conns_out[i]);
    down->conns_out[i]->trigger_downstreams(down->conns_out[i], event->intended_tag, event->super.payload,
                                            pool->payload_size);
  }

  pool->free(pool, event->super.payload);
}

// Called at the end of a logical tag if it was registered for cleanup.
void FederatedInputConnection_cleanup(Trigger* trigger) {
  LF_DEBUG(FED, "Cleaning up federated input connection %p", trigger);
  assert(trigger->is_registered_for_cleanup);
  assert(trigger->is_present);
  trigger->is_present = false;
}

void FederatedInputConnection_ctor(FederatedInputConnection* self, Reactor* parent, interval_t delay, bool is_physical,
                                   interval_t max_wait, Port** downstreams, size_t downstreams_size, void* payload_buf,
                                   bool* payload_used_buf, size_t payload_size, size_t payload_buf_capacity) {
  EventPayloadPool_ctor(&self->payload_pool, (char*)payload_buf, payload_used_buf, payload_size, payload_buf_capacity,
                        0);
  Connection_ctor(&self->super, TRIG_CONN_FEDERATED_INPUT, parent, downstreams, downstreams_size, &self->payload_pool,
                  FederatedInputConnection_prepare, FederatedInputConnection_cleanup, NULL);
  Mutex_ctor(&self->mutex.super);
  self->delay = delay;
  if (is_physical) {
    self->type = PHYSICAL_CONNECTION;
  } else {
    self->type = LOGICAL_CONNECTION;
  }
  self->last_known_tag = NEVER_TAG;
  self->last_fallback_tag = NEVER_TAG;
  self->max_wait = max_wait;
}

// Callback registered with the NetworkChannel. Is called asynchronously when there is a TaggedMessage available.
void FederatedConnectionBundle_handle_tagged_msg(FederatedConnectionBundle* self, const FederateMessage* _msg) {
  const TaggedMessage* msg = &_msg->message.tagged_message;
  LF_DEBUG(FED, "Callback on FedConnBundle %p for message of size=%u with tag:" PRINTF_TAG, self, msg->payload.size,
           msg->tag);
  assert(((size_t)msg->conn_id) < self->inputs_size);
  lf_ret_t ret;
  FederatedInputConnection* input = self->inputs[msg->conn_id];
  Environment* env = self->parent->env;
  Scheduler* sched = env->scheduler;
  EventPayloadPool* pool = &input->payload_pool;

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
  void* payload;

  // Note that we now lock the FederatedInputConnection mutex so we can read the last_know_tag from it.
  MUTEX_LOCK(input->mutex);
  ret = pool->allocate(pool, &payload);
  if (ret != LF_OK) {
    LF_ERR(FED, "Input buffer at Connection %p is full. Dropping incoming msg", input);
  } else {
    LF_INFO(FED, "Allocated payload for input %p (pool payload_size=%zu)", input, pool->payload_size);
    lf_ret_t status = (*self->deserialize_hooks[msg->conn_id])(payload, msg->payload.bytes, msg->payload.size);

    LF_INFO(FED, "Deserialization returned %d for conn %d", status, msg->conn_id);

    bool queued = false;
    if (status == LF_OK) {
      Event event = EVENT_INIT(tag, &input->super.super, payload);
      ret = sched->schedule_at(sched, &event);
      LF_INFO(FED, "First schedule_at returned %d for desired tag: " PRINTF_TAG, ret, tag);
      switch (ret) {
      case LF_AFTER_STOP_TAG:
        LF_WARN(FED, "Tried scheduling event after stop tag. Dropping");
        pool->free(pool, payload);
        break;
      case LF_PAST_TAG: {
        LF_WARN(FED, "Safe-to-process violation! Tried scheduling event to a past tag. Handling now instead!");
        // Every late message on this connection is pushed onto the current tag,
        // so two arriving within one tag would share it, both would be delivered
        // to the same input port at that tag.
        // Bump past the current tag by one microstep to avoid this.
        tag_t fallback = sched->current_tag(sched);
        if (lf_tag_compare(input->last_fallback_tag, fallback) > 0) {
          fallback = input->last_fallback_tag;
        }
        fallback.microstep++;
        event.super.tag = fallback;
        status = sched->schedule_at(sched, &event);
        LF_INFO(FED, "Second schedule_at (fallback) returned %d for tag: " PRINTF_TAG, status, event.super.tag);
        if (status == LF_OK) {
          input->last_fallback_tag = fallback;
          queued = true;
        } else {
          LF_ERR(FED, "Failed to schedule event at current tag also. Dropping");
          pool->free(pool, payload);
        }
        break;
      }
      case LF_INVALID_TAG:
        LF_WARN(FED, "Dropping event with invalid tag");
        pool->free(pool, payload);
        break;
      case LF_OK:
        queued = true;
        break;
      case LF_VALUE_BUFFER_FULL:
      // EventQueue_insert() returns LF_EVENT_QUEUE_FULL.
      case LF_EVENT_QUEUE_FULL:
        LF_ERR(FED, "EventQueue is full! desired tag: " PRINTF_TAG " current tag: " PRINTF_TAG, tag,
               env->get_logical_time(env));
        pool->free(pool, payload);
        break;
      default:
        LF_ERR(FED, "Unknown return value `%d` from schedule_at_locked", ret);
        pool->free(pool, payload);
        validate(false);
        break;
      }
    } else {
      LF_ERR(FED, "Cannot deserialize message from other Federate. Dropping");
      pool->free(pool, payload);
    }
    if (!queued) {
      pool->free(pool, payload);
    }

    if (lf_tag_compare(input->last_known_tag, tag) < 0) {
      LF_DEBUG(FED, "Updating last known tag for input %p to " PRINTF_TAG, input, tag);
      input->last_known_tag = tag;
    }
  }
  MUTEX_UNLOCK(input->mutex);
}

void FederatedConnectionBundle_msg_received_cb(FederatedConnectionBundle* self, const FederateMessage* msg) {
  FederatedEnvironment* env_fed = (FederatedEnvironment*)self->parent->env;
  switch (msg->which_message) {
  case FederateMessage_tagged_message_tag:
    LF_DEBUG(FED, "Handling tagged message");
    FederatedConnectionBundle_handle_tagged_msg(self, msg);
    break;
  case FederateMessage_startup_coordination_tag:
    LF_DEBUG(FED, "Handling start up message");
    env_fed->startup_coordinator->handle_message_callback(env_fed->startup_coordinator,
                                                          &msg->message.startup_coordination, self->index);
    break;
  case FederateMessage_shutdown_coordination_tag:
    LF_DEBUG(FED, "Handling shutdown message");
    env_fed->shutdown_coordinator->handle_message_callback(env_fed->shutdown_coordinator,
                                                           &msg->message.shutdown_coordination, self->index);
    break;
  case FederateMessage_clock_sync_msg_tag:
    LF_DEBUG(FED, "Handling clock sync message");
    if (env_fed->do_clock_sync) {
      env_fed->clock_sync->handle_message_callback(env_fed->clock_sync, &msg->message.clock_sync_msg, self->index);
    } else {
      LF_WARN(FED, "Received clock-sync message but clock-sync is disabled. Ignoring");
    }
    break;
  default:
    LF_ERR(FED, "Unknown message type %d. Dropping message.", msg->which_message);
    return;
  }
}

void FederatedConnectionBundle_ctor(FederatedConnectionBundle* self, Reactor* parent, NetworkChannel* net_channel,
                                    FederatedInputConnection** inputs, deserialize_hook* deserialize_hooks,
                                    size_t inputs_size, FederatedOutputConnection** outputs,
                                    serialize_hook* serialize_hooks, size_t outputs_size, size_t index) {
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

void FederatedConnectionBundle_validate(FederatedConnectionBundle* bundle) {
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
  }
}
