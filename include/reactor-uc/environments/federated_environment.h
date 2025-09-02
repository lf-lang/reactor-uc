#ifndef REACTOR_UC_ENVIRONMENT_FEDERATED_H
#define REACTOR_UC_ENVIRONMENT_FEDERATED_H

#include "reactor-uc/environment.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/physical_clock.h"

typedef struct FederatedEnvironment FederatedEnvironment;
extern Environment *_lf_environment; // NOLINT

struct FederatedEnvironment {
  Environment super;
  PhysicalClock clock; // The physical clock that provides the physical time.
  bool do_clock_sync;
  FederatedConnectionBundle **net_bundles; // A pointer to an array of NetworkChannel pointers that are used to
                                           // communicate with other federates running in different environments.
  size_t net_bundles_size;                 // The number of NetworkChannels in the net_channels array.
  size_t federation_longest_path;          // The longest path in the federation.
  StartupCoordinator *startup_coordinator; // A pointer to the startup coordinator, if the program has one.
  ClockSynchronization *clock_sync;        // A pointer to the clock synchronization module, if the program has one.
  interval_t global_max_wait;              // The global maximum wait time for remote input ports for this federate.

  /**
   * @brief Set the global maximum wait time for remote input ports for this federate.
   * @param super The environment.
   * @param max_wait The maximum wait time to be set.
   *
   * This function sets the global maximum wait time for remote input ports for this federate.
   */
  void (*set_maxwait)(Environment *super, interval_t max_wait);
};

void FederatedEnvironment_ctor(FederatedEnvironment *self, Reactor *main, Scheduler *scheduler, bool fast_mode,
                               FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                               StartupCoordinator *startup_coordinator, ClockSynchronization *clock_sync);
void FederatedEnvironment_free(FederatedEnvironment *self);

#define lf_set_maxwait(time) ((FederatedEnvironment *)env)->set_maxwait(env, time)

#endif
