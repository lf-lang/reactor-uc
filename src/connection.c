#include "reactor-uc/connection.h"
#include <assert.h>

Output *Connection_get_final_upstream(Connection *self) {
  assert(self->upstream);

  switch (self->upstream->super.type) {
  case OUTPUT:
    return (Output *)self->upstream;
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

void DelayedConnection_ctor(DelayedConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                            size_t num_downstreams, interval_t delay) {
  Connection_ctor(&self->super, CONN_DELAYED, parent, upstream, downstreams, num_downstreams);
  self->delay = delay;
}

void PhysicalConnection_ctor(PhysicalConnection *self, Reactor *parent, Port *upstream, Port **downstreams,
                             size_t num_downstreams, interval_t delay) {
  Connection_ctor(&self->super, CONN_PHYSICAL, parent, upstream, downstreams, num_downstreams);
  self->delay = delay;
}