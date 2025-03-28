#ifndef REACTOR_UC_ENVIRONMENT_FEDERATED_H
#define REACTOR_UC_ENVIRONMENT_FEDERATED_H

#include "reactor-uc/environment.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/physical_clock.h"

typedef struct EnvironmentFederated EnvironmentFederated;
extern Environment *_lf_environment; // NOLINT

struct EnvironmentFederated {
  Environment super;
  PhysicalClock clock; // The physical clock that provides the physical time.
  bool do_clock_sync;
  FederatedConnectionBundle **net_bundles; // A pointer to an array of NetworkChannel pointers that are used to
                                           // communicate with other federates running in different environments.
  size_t net_bundles_size;                 // The number of NetworkChannels in the net_channels array.
  size_t federation_longest_path;          // The longest path in the federation.
  StartupCoordinator *startup_coordinator; // A pointer to the startup coordinator, if the program has one.
  ClockSynchronization *clock_sync;        // A pointer to the clock synchronization module, if the program has one.
};

void EnvironmentFederated_ctor(EnvironmentFederated *self, Reactor *main, Scheduler *scheduler, bool fast_mode,
                               FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                               StartupCoordinator *startup_coordinator, ClockSynchronization *clock_sync);
void EnvironmentFederated_free(EnvironmentFederated *self);

#endif
