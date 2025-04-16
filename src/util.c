#include "reactor-uc/util.h"

void lf_connect(Connection *connection, Port *upstream, Port *downstream) {
  if (connection->upstream == NULL) {
    validate(upstream->conns_out_registered < upstream->conns_out_size);
    upstream->conns_out[upstream->conns_out_registered++] = connection;
  }
  connection->upstream = upstream;
  connection->register_downstream(connection, downstream);

  // If a connection is enclaved. We move it into the environment of the receiving federate.
  if (connection->super.type == TRIG_CONN_ENCLAVED) {
    connection->super.parent = downstream->super.parent;
  }
}

void lf_connect_federated_output(Connection *connection, Port *output) {
  validate(output->conns_out_registered < output->conns_out_size);
  output->conns_out[output->conns_out_registered++] = connection;
  connection->upstream = output;
}

void lf_connect_federated_input(Connection *connection, Port *input) {
  connection->register_downstream(connection, input);
}
