#ifndef REACTOR_UC_UTIL_H
#define REACTOR_UC_UTIL_H

#include "reactor-uc/port.h"
#include "reactor-uc/connection.h"

void lf_connect(Connection *connection, Port *upstream, Port *downstream);

void lf_connect_federated_output(Connection *connection, Port *output);

void lf_connect_federated_input(Connection *connection, Port *input);

#endif