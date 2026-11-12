#ifndef REACTOR_UC_UTIL_H
#define REACTOR_UC_UTIL_H

#include "reactor-uc/port.h"
#include "reactor-uc/connection.h"

/*!
 * @brief Connect one upstream port to one downstream port.
 */
void lf_connect(Connection* connection, Port* upstream, Port* downstream);

/*!
 * @brief Connect an output port to a federated output connection.
 */
void lf_connect_federated_output(Connection* connection, Port* output);

/*!
 * @brief Connect a federated input port to an input port.
 */
void lf_connect_federated_input(Connection* connection, Port* input);

#endif
