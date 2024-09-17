#include "reactor-uc/connection.h"
#include <assert.h>

void Connection_register_downstream(Connection *self, InputPort *port) {
  assert(self->downstreams_registered < self->downstreams_size);

  self->downstreams[self->downstreams_registered++] = port;
  port->conn = self;
  (*port->value_ptr_ptr) = self->upstream->value_ptr;
}

void Connection_ctor(Connection *self, Reactor *parent, OutputPort *upstream, InputPort **downstreams,
                     size_t num_downstreams) {
  self->parent = parent;
  self->upstream = upstream;
  self->downstreams_size = num_downstreams;
  self->downstreams_registered = 0;
  self->downstreams = downstreams;
  self->register_downstream = Connection_register_downstream;

  upstream->conn = self;
}
