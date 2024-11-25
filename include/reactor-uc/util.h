#ifndef REACTOR_UC_UTIL_H
#define REACTOR_UC_UTIL_H

#include "reactor-uc/port.h"
#include "reactor-uc/connection.h"

void lf_connect(Connection *connection, Port *upstream, Port *downstream);

#endif