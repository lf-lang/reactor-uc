#include "reactor-uc/util.h"

void lf_connect(Connection* connection, Port* upstream, Port* downstream) {
  if (connection->upstream == NULL) {
    validate(upstream->conns_out_registered < upstream->conns_out_size);
    upstream->conns_out[upstream->conns_out_registered++] = connection;
  }
  connection->upstream = upstream;
  connection->register_downstream(connection, downstream);
}

void lf_grouped_connect(Connection* connection, Port* upstream, Port* downstream_array, int downstream_size) {
  for (int i = 0; i < downstream_size; i++) {
    if (connection->upstream == NULL) {
      validate(upstream->conns_out_registered < upstream->conns_out_size);
      upstream->conns_out[upstream->conns_out_registered++] = connection;
    }
    connection->upstream = upstream;
    connection->register_downstream(connection, &downstream_array[i]);
  }
}

void lf_connect_federated_output(Connection* connection, Port* output) {
  validate(output->conns_out_registered < output->conns_out_size);
  output->conns_out[output->conns_out_registered++] = connection;
  connection->upstream = output;
}

void lf_connect_federated_input(Connection* connection, Port* input) {
  connection->register_downstream(connection, input);
}
