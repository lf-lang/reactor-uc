#include "reactor-uc/connection.h"
#include <assert.h>

OutputPort *Connection_get_final_upstream(Connection *self) {
  assert(self->upstream);

  switch (self->upstream->super.type) {
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

  if (port->super.type == INPUT) {
    OutputPort *final_upstream = self->get_final_upstream(self);
    assert(final_upstream);
    *((InputPort *)port)->value_ptr_ptr = final_upstream->value_ptr;
  }
}

void Connection_ctor(Connection *self, Reactor *parent, Port *upstream, Port **downstreams, size_t num_downstreams) {
  self->parent = parent;
  self->upstream = upstream;
  self->downstreams_size = num_downstreams;
  self->downstreams_registered = 0;
  self->downstreams = downstreams;
  self->register_downstream = Connection_register_downstream;
  self->get_final_upstream = Connection_get_final_upstream;

  upstream->conn_out = self;
}
