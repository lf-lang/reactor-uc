#include "reactor-uc/enclaved.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/environment.h"

#include <string.h>

/**
 * @brief Prepare function called from the receiving enclave when the event is handeled.
 *
 * @param trigger
 * @param event
 */
void EnclavedConnection_prepare(Trigger *trigger, Event *event) {
  LF_DEBUG(CONN, "Preparing enclave connection %p for triggering", trigger);
  EnclavedConnection *self = (EnclavedConnection *)trigger;
  EventPayloadPool *pool = trigger->payload_pool;

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

  if (self->staged_payload_ptr) {
    LF_DEBUG(CONN, "Enclaved connection %p had a staged value. Schedule it", trigger);
    Environment *receiving_env = self->super.super.parent->env;
    Environment *sending_env = self->super.upstream->super.parent->env;

    Scheduler *receiving_sched = receiving_env->scheduler;

    tag_t base_tag = ZERO_TAG;
    if (self->type == PHYSICAL_CONNECTION) {
      base_tag.time = receiving_env->get_physical_time(receiving_env);
    } else {
      // FIXME: When federated support is added, we must check whether this enclaved connection
      // is connected to a federated input port. If this is the case, we should use the `intended_tag`
      // of the federated input port. Not the current tag of the scheduler.
      base_tag = sending_env->scheduler->current_tag(sending_env->scheduler);
    }
    tag_t tag = lf_delay_tag(base_tag, self->delay);
    Event event = EVENT_INIT(tag, &self->super.super, self->staged_payload_ptr);

    lf_ret_t ret = receiving_sched->schedule_at(receiving_sched, &event);

    if (ret == LF_PAST_TAG) {
      // STP-violation
      LF_WARN(CONN, "STP violation detected in enclaved connection.");
      ret = receiving_sched->schedule_at_earilest_possible_tag(receiving_sched, &event);
      validate(ret == LF_OK);
    }

    self->set_last_known_tag(self, event.super.tag);

    self->staged_payload_ptr = NULL;
  }
}

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

/** Returns the latest known tag of the connection. Used if we have specified a maxwait. */
tag_t EnclavedConnection_get_last_known_tag(EnclavedConnection *self) {
  tag_t res;
  MUTEX_LOCK(self->mutex);
  res = self->_last_known_tag;
  MUTEX_UNLOCK(self->mutex);
  return res;
}

/** Sets the latest known tag of this connection. Used if we have specified a maxwait. */
void EnclavedConnection_set_last_known_tag(EnclavedConnection *self, tag_t tag) {
  MUTEX_LOCK(self->mutex);
  self->_last_known_tag = tag;
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
  self->_last_known_tag = NEVER_TAG;
  self->get_last_known_tag = EnclavedConnection_get_last_known_tag;
  self->set_last_known_tag = EnclavedConnection_set_last_known_tag;
}