#ifndef REACTOR_UC_UTIL_H
#define REACTOR_UC_UTIL_H

#include "reactor-uc/port.h"
#include "reactor-uc/connection.h"

void connect(Connection *connection, Port *upstream, Port *downstream) {
  if (connection->upstream == NULL) {
    validate(upstream->conns_out_registered < upstream->conns_out_size);
    upstream->conns_out[upstream->conns_out_registered++] = connection;
  }
  connection->upstream = upstream;
  connection->register_downstream(connection, downstream);
}

void connect_single(Connection ***connection, Port ***upstream, int upstream_bank_width, int upstream_port_width,
                    Port ***downstream, int downstream_bank_width, int downstream_port_width) {
  for (int i = 0; i < downstream_bank_width; i++) {
    for (int j = 0; j < downstream_port_width; j++) {
      connect(connection[i][j], upstream[i][j], downstream[i][j]);
    }
  }
}

void connect_multiple(Connection ****connection, Port ****upstreams, size_t *upstreams_bank_width,
                      size_t upstreams_port_width, size_t upstreams_size, Port ****downstreams,
                      size_t *downstreams_bank_width, size_t *downstreams_port_width, size_t downstreams_size,
                      bool repeated) {
  size_t downstreams_idx = 0;
  size_t downstream_bank_idx = 0;
  size_t downstream_port_idx = 0;

  do {
    for (size_t i = 0; i < upstreams_size; i++) {
      size_t bank_width = upstreams_bank_width[i];
      for (size_t j = 0; j < bank_width; j++) {
        size_t port_width = upstreams_port_width;
        for (size_t k = 0; k < port_width; k++) {
          connect(connection[i][j][k], upstreams[i][j][k],
                  downstreams[downstreams_idx][downstream_bank_idx][downstream_port_idx]);

          downstream_port_idx++;
          if (downstream_port_idx == downstreams_port_width[downstreams_idx]) {
            downstream_port_idx = 0;
            downstream_bank_idx++;
          }

          if (downstream_bank_idx == downstreams_bank_width[downstreams_idx]) {
            downstream_bank_idx = 0;
            downstreams_idx++;
          }

          if (downstreams_idx == downstreams_size) {
            return;
          }
        }
      }
    }
  } while(repeated);
}

#endif