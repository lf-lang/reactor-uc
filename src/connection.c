#include "reactor-uc/connection.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/payload_pool.h"
#include <assert.h>
#include <string.h>

Output *Connection_get_final_upstream(Connection *self) {

  if (!self->upstream) {
    return NULL;
  }

  switch (self->upstream->super.type) {
  case TRIG_OUTPUT:
    return (Output *)self->upstream;
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
  validate(self->downstreams_registered < self->downstreams_size);
  LF_DEBUG(CONN, "Registering downstream %p with connection %p", port, self);

  self->downstreams[self->downstreams_registered++] = port;
  port->conn_in = self;
}

/**
 * @brief Recursively walks down connection graph and copies value into Input ports and triggers reactions.
 */
void LogicalConnection_trigger_downstreams(Connection *self, const void *value, size_t value_size) {
  LF_DEBUG(CONN, "Triggering downstreams of %p with value %p", self, value);
  for (size_t i = 0; i < self->downstreams_size; i++) {
    Port *down = self->downstreams[i];

    if (down->super.type == TRIG_INPUT) {
      LF_DEBUG(CONN, "Found downstream input port %p to trigger.", down);
      Input *inp = (Input *)down;
      validate(value_size == inp->value_size);
      memcpy(inp->value_ptr, value, value_size); // NOLINT
      // Only call `prepare` and thus trigger downstream reactions once per
      // tag. This is to support multiple writes to the same port with
      // "last write wins"
      if (!inp->super.super.is_present) {
        inp->super.super.prepare(&inp->super.super);
      }
    }

    if (down->conn_out) {
      LF_DEBUG(CONN, "Found further downstream connection %p to recurse down", down->conn_out);
      down->conn_out->trigger_downstreams(down->conn_out, value, value_size);
    }
  }
}

void Connection_ctor(Connection *self, TriggerType type, Reactor *parent, Port **downstreams, size_t num_downstreams,
                     EventPayloadPool *payload_pool, void (*prepare)(Trigger *), void (*cleanup)(Trigger *),
                     void (*trigger_downstreams)(Connection *, const void *, size_t)) {

  self->upstream = NULL;
  self->downstreams_size = num_downstreams;
  self->downstreams_registered = 0;
  self->downstreams = downstreams;
  self->register_downstream = Connection_register_downstream;
  self->get_final_upstream = Connection_get_final_upstream;
  self->trigger_downstreams = trigger_downstreams;

  Trigger_ctor(&self->super, type, parent, payload_pool, prepare, cleanup, NULL);
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
void DelayedConnection_prepare(Trigger *trigger) {
  LF_DEBUG(CONN, "Preparing delayed connection %p for triggering", trigger);
  DelayedConnection *self = (DelayedConnection *)trigger;
  Scheduler *sched = &trigger->parent->env->scheduler;
  EventPayloadPool *tval = &self->payload_pool;
  void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];
  trigger->is_present = true;

  sched->register_for_cleanup(sched, trigger);

  LogicalConnection_trigger_downstreams(&self->super, value_ptr, self->payload_pool.value_size);
}

/**
 * @brief Called at the end of logical tags. In charge of two things:
 * 1. Increment `read_idx` of the EventPayloadPool FIFO through the `pop` call.
 * 2. Increment the `write_idx` of the EventPayloadPool FIFO (`push`) and schedule
 * an event based on the delay of this connection.
 */
void DelayedConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(CONN, "Cleaning up delayed connection %p", trigger);
  DelayedConnection *self = (DelayedConnection *)trigger;
  validate(trigger->is_registered_for_cleanup);

  if (trigger->is_present) {
    LF_DEBUG(CONN, "Delayed connection %p had a present value this tag. Pop it", trigger);
    trigger->is_present = false;
    int ret = self->payload_pool.pop(&self->payload_pool);
    validaten(ret);
  }

  if (self->payload_pool.staged) {
    LF_DEBUG(CONN, "Delayed connection %p had a staged value. Schedule it", trigger);
    Environment *env = self->super.super.parent->env;
    Scheduler *sched = &env->scheduler;

    tag_t tag = lf_delay_tag(sched->current_tag, self->delay);
    self->payload_pool.push(&self->payload_pool);
    sched->schedule_at(sched, trigger, tag);
  }
}

/**
 * @brief Attempts to trigger downstreams are stopped when we arrive at a delayed connection.
 * Instead we stage the value for scheduling at the end of the current tag.
 */
void DelayedConnection_trigger_downstreams(Connection *_self, const void *value, size_t value_size) {
  (void)value_size;
  LF_DEBUG(CONN, "Triggering downstreams on delayed connection %p. Stage the value for later scheduling", _self);
  DelayedConnection *self = (DelayedConnection *)_self;
  Scheduler *sched = &_self->super.parent->env->scheduler;

  if (value) {
    self->payload_pool.stage(&self->payload_pool, value);
  } else {
    throw("DelayedConnection with untyped value");
  }
  sched->register_for_cleanup(sched, &_self->super);
}

void DelayedConnection_ctor(DelayedConnection *self, Reactor *parent, Port **downstreams, size_t num_downstreams,
                            interval_t delay, void *value_buf, size_t value_size, size_t value_capacity) {
  self->delay = delay;
  EventPayloadPool_ctor(&self->payload_pool, value_buf, value_size, value_capacity);
  Connection_ctor(&self->super, TRIG_CONN_DELAYED, parent, downstreams, num_downstreams, &self->payload_pool,
                  DelayedConnection_prepare, DelayedConnection_cleanup, DelayedConnection_trigger_downstreams);
}

void PhysicalConnection_prepare(Trigger *trigger) {
  LF_DEBUG(CONN, "Preparing physical connection %p for triggering", trigger);
  PhysicalConnection *self = (PhysicalConnection *)trigger;
  Scheduler *sched = &trigger->parent->env->scheduler;
  EventPayloadPool *tval = &self->payload_pool;
  void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];
  trigger->is_present = true;

  sched->register_for_cleanup(sched, trigger);

  LogicalConnection_trigger_downstreams(&self->super, value_ptr, self->payload_pool.value_size);
}

void PhysicalConnection_cleanup(Trigger *trigger) {
  LF_DEBUG(CONN, "Cleaning up physical connection %p", trigger);
  PhysicalConnection *self = (PhysicalConnection *)trigger;
  validate(trigger->is_registered_for_cleanup);

  if (trigger->is_present) {
    LF_DEBUG(CONN, "Physical connection %p had a present value this tag. Pop it", trigger);
    trigger->is_present = false;
    int ret = self->payload_pool.pop(&self->payload_pool);
    validate(ret == 0);
  }

  if (self->payload_pool.staged) {
    LF_DEBUG(CONN, "Physical connection %p had a staged value. Schedule it", trigger);
    Environment *env = self->super.super.parent->env;
    Scheduler *sched = &env->scheduler;

    tag_t now_tag = {.time = env->get_physical_time(env), .microstep = 0};
    tag_t tag = lf_delay_tag(now_tag, self->delay);
    validaten(self->payload_pool.push(&self->payload_pool));
    validaten(sched->schedule_at(sched, trigger, tag));
  }
}

void PhysicalConnection_trigger_downstreams(Connection *_self, const void *value, size_t value_size) {
  LF_DEBUG(CONN, "Triggering downstreams on physical connection %p. Stage value for later scheduling", _self);
  (void)value_size;
  PhysicalConnection *self = (PhysicalConnection *)_self;
  Scheduler *sched = &_self->super.parent->env->scheduler;
  validate(value);

  // Try to stage the value for scheduling.
  int res = self->payload_pool.stage(&self->payload_pool, value);
  if (res == LF_OK) {
    sched->register_for_cleanup(sched, &_self->super);
  }
  // Possibly handle
}

void PhysicalConnection_ctor(PhysicalConnection *self, Reactor *parent, Port **downstreams, size_t num_downstreams,
                             interval_t delay, void *value_buf, size_t value_size, size_t value_capacity) {
  EventPayloadPool_ctor(&self->payload_pool, value_buf, value_size, value_capacity);
  Connection_ctor(&self->super, TRIG_CONN_PHYSICAL, parent, downstreams, num_downstreams, &self->payload_pool,
                  PhysicalConnection_prepare, PhysicalConnection_cleanup, PhysicalConnection_trigger_downstreams);
  self->delay = delay;
}
