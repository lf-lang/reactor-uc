#include "reactor-uc/connection.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/trigger_value.h"
#include <assert.h>
#include <string.h>

Output *Connection_get_final_upstream(Connection *self) {
  assert(self->upstream);

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
    assert(false);
  }
}

void Connection_register_downstream(Connection *self, Port *port) {
  assert(self->downstreams_registered < self->downstreams_size);

  self->downstreams[self->downstreams_registered++] = port;
  port->conn_in = self;
}

void Connection_trigger_downstreams(Connection *self, const void *value, size_t value_size) {
  for (size_t i = 0; i < self->downstreams_size; i++) {
    Port *down = self->downstreams[i];

    if (down->super.type == TRIG_INPUT) {
      Input *inp = (Input *)down;
      assert(value_size == inp->value_size);
      memcpy(inp->value_ptr, value, value_size);
      inp->super.super.prepare(&inp->super.super);
    }

    if (down->conn_out) {
      down->conn_out->trigger_downstreams(down->conn_out, value, value_size);
    }
  }
}

void Connection_ctor(Connection *self, TriggerType type, Reactor *parent, Port *upstream, Port **downstreams,
                     size_t num_downstreams, TriggerValue *trigger_value, void (*prepare)(Trigger *),
                     void (*cleanup)(Trigger *), void (*trigger_downstreams)(Connection *, const void *, size_t)) {
  upstream->conn_out = self;
  self->upstream = upstream;
  self->downstreams_size = num_downstreams;
  self->downstreams_registered = 0;
  self->downstreams = downstreams;
  self->register_downstream = Connection_register_downstream;
  self->get_final_upstream = Connection_get_final_upstream;
  if (trigger_downstreams) {
    self->trigger_downstreams = trigger_downstreams;
  } else {
    self->trigger_downstreams = Connection_trigger_downstreams;
  }

  SchedulableTrigger_ctor(&self->super, type, parent, trigger_value, prepare, cleanup);
}

void DelayedConnection_prepare(Trigger *trigger) {
  DelayedConnection *self = (DelayedConnection *)trigger;
  TriggerValue *tval = &self->trigger_value;
  trigger->is_present = true;
  void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];
  Connection_trigger_downstreams(&self->super, value_ptr, self->trigger_value.value_size);
}

// FIXME: How to make sure that this is also called? We need to put the Trigger pointer on the
// reactor struct I guess?
void DelayedConnection_cleanup(Trigger *trigger) {
  DelayedConnection *self = (DelayedConnection *)trigger;
  trigger->is_present = false;
  int ret = self->trigger_value.pop(&self->trigger_value);
  assert(ret == 0);
}

void DelayedConnection_trigger_downstreams(Connection *_self, const void *value, size_t value_size) {
  (void)value_size;
  DelayedConnection *self = (DelayedConnection *)_self;
  SchedulableTrigger *trigger = &self->super.super;
  Environment *env = self->super.parent->env;

  tag_t tag = lf_delay_tag(env->current_tag, self->delay);
  trigger->schedule_at(trigger, tag, value);
}

void DelayedConnection_ctor(DelayedConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                            size_t num_downstreams, interval_t delay, void *value_buf, size_t value_size,
                            size_t value_capacity) {
  self->delay = delay;
  TriggerValue_ctor(&self->trigger_value, value_buf, value_size, value_capacity);
  Connection_ctor(&self->super, TRIG_CONN_DELAYED, parent, upstream, downstreams, num_downstreams, &self->trigger_value,
                  DelayedConnection_prepare, DelayedConnection_cleanup, DelayedConnection_trigger_downstreams);
}

void PhysicalConnection_prepare(Trigger *trigger) {
  DelayedConnection *self = (DelayedConnection *)trigger;
  TriggerValue *tval = &self->trigger_value;
  trigger->is_present = true;
  for (size_t i = 0; i < self->super.downstreams_size; i++) {
    Port *down = self->super.downstreams[i];
    void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];

    if (down->super.type == TRIG_INPUT) {
      Input *inp = (Input *)down;
      memcpy(inp->value_ptr, value_ptr, inp->value_size);
      inp->super.super.prepare(&inp->super.super);
    }

    down->copy_value_and_schedule_downstreams(down, value_ptr);
  }
}

// FIXME: How to make sure that this is also called? We need to put the Trigger pointer on the
// reactor struct I guess?
void PhysicalConnection_cleanup(Trigger *trigger) {
  DelayedConnection *self = (DelayedConnection *)trigger;
  trigger->is_present = false;
  int ret = self->trigger_value.pop(&self->trigger_value);
  assert(ret == 0);
}

void PhysicalConnection_trigger_downstreams(Connection *_self, const void *value, size_t value_size) {
  (void)value_size;
  PhysicalConnection *self = (PhysicalConnection *)_self;
  SchedulableTrigger *trigger = &self->super.super;
  Environment *env = self->super.parent->env;
  tag_t current_time = {.time = env->get_physical_time(env), .microstep = 0};
  tag_t tag = lf_delay_tag(current_time, self->delay);
  trigger->schedule_at(trigger, tag, value);
}

void PhysicalConnection_ctor(PhysicalConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                             size_t num_downstreams, interval_t delay, void *value_buf, size_t value_size,
                             size_t value_capacity) {
  TriggerValue_ctor(&self->trigger_value, value_buf, value_size, value_capacity);
  Connection_ctor(&self->super, TRIG_CONN_PHYSICAL, parent, upstream, downstreams, num_downstreams,
                  &self->trigger_value, PhysicalConnection_prepare, PhysicalConnection_cleanup,
                  PhysicalConnection_trigger_downstreams);
  self->delay = delay;
}
