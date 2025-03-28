#include "reactor-uc/environments/environment_federated.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/startup_coordinator.h"
#include <assert.h>
#include <inttypes.h>

void EnvironmentFederated_validate(Environment *super) {
  EnvironmentFederated *self = (EnvironmentFederated *)super;
  Reactor_validate(super->main);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle_validate(self->net_bundles[i]);
  }
}

void EnvironmentFederated_assemble(Environment *super) {
  EnvironmentFederated *self = (EnvironmentFederated *)super;
  // Here we enter a critical section which do not leave.
  // The scheduler will leave the critical section before executing the reactions.
  // Everything else within the runtime happens in a critical section.
  super->enter_critical_section(super);
  validaten(super->main->calculate_levels(super->main));
  lf_ret_t ret;
  EnvironmentFederated_validate(super);

  // Establish connections to all neighbors:
  ret = self->startup_coordinator->connect_to_neighbors_blocking(self->startup_coordinator);
  validate(ret == LF_OK);
  self->startup_coordinator->start(self->startup_coordinator);
}

void EnvironmentFederated_start(Environment *super) {
  // We do not set the start time here in federated mode, instead the StartupCoordinator will do it.
  // So we just start the main loop and the StartupCoordinator.
  super->scheduler->run(super->scheduler);
}

lf_ret_t EnvironmentFederated_wait_until_locked(Environment *super, instant_t wakeup_time) {
  EnvironmentFederated *self = (EnvironmentFederated *)super;
  if (wakeup_time <= super->get_physical_time(super) || super->fast_mode) {
    return LF_OK;
  }

  // Translate the wakeup time into the time scale used by the platform.
  // This is different from the application-level time scale if we are running
  // distributed with clock-sync enabled.
  instant_t hw_wakeup_time = self->clock.to_hw_time(&self->clock, wakeup_time);

  if (super->has_async_events) {
    return super->platform->wait_until_interruptible_locked(super->platform, hw_wakeup_time);
  } else {
    return super->platform->wait_until(super->platform, hw_wakeup_time);
  }
}

interval_t EnvironmentFederated_get_physical_time(Environment *super) {
  EnvironmentFederated *self = (EnvironmentFederated *)super;
  return self->clock.get_time(&self->clock);
}

void EnvironmentFederated_ctor(EnvironmentFederated *self, Reactor *main, Scheduler *scheduler, bool fast_mode,
                               FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                               StartupCoordinator *startup_coordinator, ClockSynchronization *clock_sync) {
  Environment_ctor(&self->super, main, scheduler, fast_mode);
  self->super.assemble = EnvironmentFederated_assemble;
  self->super.start = EnvironmentFederated_start;
  self->super.wait_until_locked = EnvironmentFederated_wait_until_locked;
  self->super.get_physical_time = EnvironmentFederated_get_physical_time;
  self->net_bundles_size = net_bundles_size;
  self->net_bundles = net_bundles;
  self->startup_coordinator = startup_coordinator;
  self->clock_sync = clock_sync;
  self->do_clock_sync = clock_sync != NULL;
  PhysicalClock_ctor(&self->clock, &self->super, self->do_clock_sync);

  self->super.has_async_events = true;

  validate(self->net_bundles_size > 0);
  validate(self->net_bundles);
  validate(self->startup_coordinator);
}

void EnvironmentFederated_free(EnvironmentFederated *self) {
  LF_INFO(ENV, "Reactor shutting down, freeing environment.");
  Environment_free(&self->super);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    NetworkChannel *chan = self->net_bundles[i]->net_channel;
    chan->free(chan);
  }
}