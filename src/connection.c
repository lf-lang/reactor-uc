#include "reactor-uc/connection.h"
#include "reactor-uc/trigger_value.h"
#include <assert.h>
#include <string.h>

OutputPort *Connection_get_final_upstream(Connection *self) {
  assert(self->upstream);

  switch (self->upstream->type) {
  case OUTPUT:
    return (OutputPort *)self->upstream;
  case INPUT:
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

void Connection_ctor(Connection *self, ConnectionType type, Reactor *parent, Port *upstream, Port **downstreams,
                     size_t num_downstreams) {
  self->parent = parent;
  self->type = type;
  self->upstream = upstream;
  self->downstreams_size = num_downstreams;
  self->downstreams_registered = 0;
  self->downstreams = downstreams;
  self->register_downstream = Connection_register_downstream;
  self->get_final_upstream = Connection_get_final_upstream;

  upstream->conn_out = self;
}

void DelayedConnection_prepare(Trigger *trigger) {
  DelayedConnection *self = (DelayedConnection *)trigger->friend;
  TriggerValue *tval = &self->trigger_value;
  trigger->is_present = true;
  for (size_t i = 0; i < self->super.downstreams_size; i++) {
    Port *down = self->super.downstreams[i];
    void *value_ptr = (void *)&tval->buffer[tval->read_idx * tval->value_size];

    // FIXME: Here we are copying the value into the input port
    if (down->type == INPUT) {
      InputPort *inp = (InputPort *)down;
      memcpy(inp->value_ptr, value_ptr, inp->value_size);
      inp->prepare(inp);
    }

    down->copy_value_and_schedule_downstreams(down, value_ptr);
  }
}

// FIXME: How to make sure that this is also called? We need to put the Trigger pointer on the
// reactor struct I guess?
void DelayedConnection_cleanup(Trigger *trigger) {
  DelayedConnection *self = (DelayedConnection *)trigger->friend;
  trigger->is_present = false;
  int ret = self->trigger_value.pop(&self->trigger_value);
  assert(ret == 0);
}

void DelayedConnection_ctor(DelayedConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                            size_t num_downstreams, interval_t delay, void *value_buf, size_t value_size,
                            size_t value_capacity) {
  TriggerValue_ctor(&self->trigger_value, value_buf, value_size, value_capacity);
  Connection_ctor(&self->super, CONN_DELAYED, parent, upstream, downstreams, num_downstreams);
  Trigger_ctor(&self->trigger, TRIG_CONN_DELAYED, parent, NULL, 0, NULL, 0, &self->trigger_value, (void *)self,
               DelayedConnection_prepare, DelayedConnection_cleanup);
  self->delay = delay;
}

void PhysicalConnection_ctor(PhysicalConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                             size_t num_downstreams, interval_t delay) {
  Connection_ctor(&self->super, CONN_PHYSICAL, parent, upstream, downstreams, num_downstreams);
  self->delay = delay;
}
