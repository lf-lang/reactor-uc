#include "reactor-uc/connection.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/event.h"
#include "reactor-uc/logging.h"
#include <assert.h>
#include <string.h>

Port *Connection_get_final_upstream(Connection *self) {

  if (!self->upstream || self->super.type != TRIG_CONN) {
    return NULL;
  }

  switch (self->upstream->super.type) {
  case TRIG_OUTPUT:
    if (self->upstream->conn_in) {
      return Connection_get_final_upstream(self->upstream->conn_in);
    } else {
      return self->upstream;
    }
  case TRIG_INPUT:
    if (self->upstream->conn_in) {
      return Connection_get_final_upstream(self->upstream->conn_in);
    } else {
      return NULL;
    }
  default:
    return NULL;
  }
}

void Connection_register_downstream(Connection *self, Port *port) {
  LF_DEBUG(CONN, "Registering downstream %p with connection %p", port, self);
  validate(self->downstreams_registered < self->downstreams_size);

  self->downstreams[self->downstreams_registered++] = port;
  port->conn_in = self;
}

/**
 * @brief Recursively walks down connection graph and copies value into Input ports and triggers reactions.
 */
void LogicalConnection_trigger_downstreams(Connection *self, const void *value, size_t value_size) {
  LF_DEBUG(CONN, "Triggering downstreams of %p with value %p", self, value);
  for (size_t i = 0; i < self->downstreams_registered; i++) {
    Port *down = self->downstreams[i];

    if (down->effects.size > 0 || down->observers.size > 0) {
      validate(value_size == down->value_size);
      memcpy(down->value_ptr, value, value_size); // NOLINT

      // Only call `prepare` and thus trigger downstream reactions once per
      // tag. This is to support multiple writes to the same port with
      // "last write wins"
      if (!down->super.is_present) {
        down->super.prepare(&down->super, NULL);
      }
    }

    for (size_t i = 0; i < down->conns_out_registered; i++) {
      LF_DEBUG(CONN, "Found further downstream connection %p to recurse down", down->conns_out[i]);
      down->conns_out[i]->trigger_downstreams(down->conns_out[i], value, value_size);
    }
  }
}

void Connection_ctor(Connection *self, TriggerType type, Reactor *parent, Port **downstreams, size_t num_downstreams,
                     EventPayloadPool *payload_pool, void (*prepare)(Trigger *, Event *), void (*cleanup)(Trigger *),
                     void (*trigger_downstreams)(Connection *, const void *, size_t)) {

  self->upstream = NULL;
  self->downstreams_size = num_downstreams;
  self->downstreams_registered = 0;
  self->downstreams = downstreams;
  self->register_downstream = Connection_register_downstream;
  self->get_final_upstream = Connection_get_final_upstream;
  self->trigger_downstreams = trigger_downstreams;

  Trigger_ctor(&self->super, type, parent, payload_pool, prepare, cleanup);
}

void LogicalConnection_ctor(LogicalConnection *self, Reactor *parent, Port **downstreams, size_t num_downstreams) {
  Connection_ctor(&self->super, TRIG_CONN, parent, downstreams, num_downstreams, NULL, NULL, NULL,
                  LogicalConnection_trigger_downstreams);
}

/**
 * @brief This is called when the event associated with a delayed connection is
 * handled. Then we can follow down the connection graph and copy the buffered
 * value into the Input ports and trigger reactions.
 *
 * @param trigger
 */
void DelayedConnection_prepare(Trigger *trigger, Event *event) {
  LF_DEBUG(CONN, "Preparing delayed connection %p for triggering", trigger);
  DelayedConnection *self = (DelayedConnection *)trigger;
  assert(self->staged_payload_ptr == NULL); // Should be reset to NULL at end of last tag.
  Scheduler *sched = trigger->parent->env->scheduler;
  EventPayloadPool *pool = trigger->payload_pool;
  trigger->is_present = true;
  sched->register_for_cleanup(sched, trigger);

  LogicalConnection_trigger_downstreams(&self->super, event->super.payload, pool->payload_size);
  validate(pool->free(pool, event->super.payload) == LF_OK);
}

void DelayedConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(CONN, "Cleaning up delayed connection %p", trigger);
  DelayedConnection *self = (DelayedConnection *)trigger;
  validate(trigger->is_registered_for_cleanup);

  if (trigger->is_present) {
    LF_DEBUG(CONN, "Delayed connection %p had a present value this tag. Pop it", trigger);
    trigger->is_present = false;
  }

  if (self->staged_payload_ptr) {
    LF_DEBUG(CONN, "Delayed connection %p had a staged value. Schedule it", trigger);
    Environment *env = self->super.super.parent->env;
    Scheduler *sched = env->scheduler;

    tag_t base_tag = ZERO_TAG;
    if (self->type == PHYSICAL_CONNECTION) {
      base_tag.time = env->get_physical_time(env);
    } else {
      base_tag = sched->current_tag(sched);
    }
    tag_t tag = lf_delay_tag(base_tag, self->delay);
    Event event = EVENT_INIT(tag, &self->super.super, self->staged_payload_ptr);
    sched->schedule_at(sched, &event);
    self->staged_payload_ptr = NULL;
  }
}

void DelayedConnection_trigger_downstreams(Connection *_self, const void *value, size_t value_size) {
  DelayedConnection *self = (DelayedConnection *)_self;
  assert(value);
  assert(value_size > 0);
  lf_ret_t ret;
  LF_DEBUG(CONN, "Triggering downstreams on delayed connection %p. Stage the value for later scheduling", _self);
  Trigger *trigger = &_self->super;
  Scheduler *sched = _self->super.parent->env->scheduler;
  EventPayloadPool *pool = trigger->payload_pool;
  if (self->staged_payload_ptr == NULL) {
    ret = pool->allocate(pool, &self->staged_payload_ptr);
    if (ret != LF_OK) {
      LF_ERR(CONN, "No more space in event buffer for delayed connection %p, dropping. Capacity is %d", _self,
             self->payload_pool.capacity);
      return;
    }
  }
  memcpy(self->staged_payload_ptr, value, value_size);
  sched->register_for_cleanup(sched, &_self->super);
}

void DelayedConnection_ctor(DelayedConnection *self, Reactor *parent, Port **downstreams, size_t num_downstreams,
                            interval_t delay, ConnectionType type, size_t payload_size, void *payload_buf,
                            bool *payload_used_buf, size_t payload_buf_capacity) {

  self->delay = delay;
  self->staged_payload_ptr = NULL;
  self->type = type;
  EventPayloadPool_ctor(&self->payload_pool, (char *)payload_buf, payload_used_buf, payload_size, payload_buf_capacity,
                        0);
  Connection_ctor(&self->super, TRIG_CONN_DELAYED, parent, downstreams, num_downstreams, &self->payload_pool,
                  DelayedConnection_prepare, DelayedConnection_cleanup, DelayedConnection_trigger_downstreams);
}

/**
 * @brief Prepare function called from the receiving enclave when the event is handeled.
 *
 * @param trigger
 * @param event
 */
void EnclavedConnection_prepare(Trigger *trigger, Event *event) {
  LF_DEBUG(CONN, "Preparing enclave connection %p for triggering", trigger);
  EnclavedConnection *self = (EnclavedConnection *)trigger;
  Environment *env = trigger->parent->env;
  Scheduler *sched = env->scheduler;
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

/**
 * @brief Cleanup function called from the sending enclave at the end of a tag when it has written to this connection.
 * It should schedule the value onto the event queue of the receiving enclave.
 *
 * @param trigger
 */
void EnclavedConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(CONN, "Cleaning up Enclaved connection %p", trigger);
  EnclavedConnection *self = (EnclavedConnection *)trigger;
  validate(trigger->is_registered_for_cleanup);

  // FIXME: Can we remove this?
  if (trigger->is_present) {
    LF_DEBUG(CONN, "Enclaved connection %p had a present value this tag. Pop it", trigger);
    trigger->is_present = false;
  }

  if (self->staged_payload_ptr) {
    LF_DEBUG(CONN, "Enclaved connection %p had a staged value. Schedule it", trigger);
    Environment *receiving_env = self->super.super.parent->env;
    Environment *sending_env = self->super.upstream->super.parent->env;

    Scheduler *receiving_sched = receiving_env->scheduler;

    // FIXME: Handle STP violations.
    tag_t base_tag = ZERO_TAG;
    if (self->type == PHYSICAL_CONNECTION) {
      base_tag.time = receiving_env->get_physical_time(receiving_env);
    } else {
      base_tag = sending_env->scheduler->current_tag(sending_env->scheduler);
    }
    tag_t tag = lf_delay_tag(base_tag, self->delay);
    Event event = EVENT_INIT(tag, &self->super.super, self->staged_payload_ptr);

    printf("%i schedule event to %i at " PRINTF_TIME ".\n", sending_env->id, receiving_env->id, event.super.tag.time);

    lf_ret_t ret = receiving_sched->schedule_at(receiving_sched, &event);

    // FIXME: There is a race condition here. Actually we need to acquire the lock on the receiving enclave.
    // also solved with adding a `schedule_now` function.
    if (ret == LF_PAST_TAG) {
      // STP-violation
      LF_WARN(CONN, "STP violation");
      event.super.tag = lf_delay_tag(receiving_sched->current_tag(receiving_sched), 0);
      ret = receiving_sched->schedule_at(receiving_sched, &event);
      validate(ret == LF_OK);
    }

    self->set_last_known_tag(self, event.super.tag);

    self->staged_payload_ptr = NULL;
  }
}

// FIXME: How do we deal with a connection going to multiple downstreams?
/**
 * @brief This function is called from the context of the sending enclave and
 * schedules an event onto the event queue of the receiving enclave.
 *
 * @param super A pointer to the Connection object. Belongs to the receiving enclave.
 * @param value A poiner to the value written over the connection.
 * @param value_size The size of the value written over the connection.
 */
void EnclavedConnection_trigger_downstreams(Connection *super, const void *value, size_t value_size) {
  assert(value);
  assert(value_size > 0);
  EnclavedConnection *self = (EnclavedConnection *)super;
  lf_ret_t ret;
  LF_DEBUG(CONN, "Triggering downstreams on Enclaved connection %p. Stage the value for later scheduling", super);
  EventPayloadPool *pool = super->super.payload_pool;
  if (self->staged_payload_ptr == NULL) {
    ret = pool->allocate(pool, &self->staged_payload_ptr);
    if (ret != LF_OK) {
      LF_ERR(CONN, "No more space in event buffer for Enclaved connection %p, dropping. Capacity is %d", super,
             self->payload_pool.capacity);
      return;
    }
  }
  memcpy(self->staged_payload_ptr, value, value_size);

  // Note that we register this trigger for cleanup with the sending scheduler. This trigger does belong to
  // the receiving scheduler. But it is within the cleanup function that the value is scheduled.
  Scheduler *sending_scheduler = super->upstream->super.parent->env->scheduler;
  sending_scheduler->register_for_cleanup(sending_scheduler, &super->super);
}

tag_t EnclavedConnection_get_last_known_tag(EnclavedConnection *self) {
  tag_t res;
  MUTEX_LOCK(self->mutex);
  res = self->last_known_tag;
  MUTEX_UNLOCK(self->mutex);
  return res;
}

void EnclavedConnection_set_last_known_tag(EnclavedConnection *self, tag_t tag) {
  MUTEX_LOCK(self->mutex);
  self->last_known_tag = tag;
  MUTEX_UNLOCK(self->mutex);
}

void EnclavedConnection_ctor(EnclavedConnection *self, Reactor *parent, Port **downstreams, size_t num_downstreams,
                             interval_t delay, ConnectionType type, size_t payload_size, void *payload_buf,
                             bool *payload_used_buf, size_t payload_buf_capacity) {

  self->delay = delay;
  self->staged_payload_ptr = NULL;
  self->type = type;

  EventPayloadPool_ctor(&self->payload_pool, (char *)payload_buf, payload_used_buf, payload_size, payload_buf_capacity,
                        0);
  Connection_ctor(&self->super, TRIG_CONN_ENCLAVED, parent, downstreams, num_downstreams, &self->payload_pool,
                  EnclavedConnection_prepare, EnclavedConnection_cleanup, EnclavedConnection_trigger_downstreams);

  Mutex_ctor(&self->mutex.super);
  self->last_known_tag = NEVER_TAG;
  self->get_last_known_tag = EnclavedConnection_get_last_known_tag;
  self->set_last_known_tag = EnclavedConnection_set_last_known_tag;
}
