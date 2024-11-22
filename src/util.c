#include "reactor-uc/util.h"

void lf_connect(Connection *connection, Port *upstream, Port *downstream) {
  if (connection->upstream == NULL) {
    validate(upstream->conns_out_registered < upstream->conns_out_size);
    upstream->conns_out[upstream->conns_out_registered++] = connection;
  }
  connection->upstream = upstream;
  connection->register_downstream(connection, downstream);
}
