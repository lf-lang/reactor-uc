#ifndef REACTOR_UC_STARTUP_COORDINATOR_H
#define REACTOR_UC_STARTUP_COORDINATOR_H

#include "reactor-uc/error.h"
#include "proto/message.pb.h"

typedef StartupCoordinator StartupCoordinator;
typedef Environment Environment;

struct StartupCoordinator {
  Environment *env;

  void (*handle_message)(StartupCoordinator *self, const FederateMessage *msg);
  lf_ret_t (*perform_handshake)(StartupCoordinator *self);
  lf_ret_t (*negotiate_start_tag)(StartupCoordinator *self);
};

void StartupCoordinator_ctor(StartupCoordinator *self, Environment *env);

#endif // REACTOR_UC_STARTUP_COORDINATOR_H