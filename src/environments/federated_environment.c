#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/startup_coordinator.h"
#include <assert.h>
#include <inttypes.h>

static void FederatedEnvironment_validate(Environment *super) {
  FederatedEnvironment *self = (FederatedEnvironment *)super;
  Reactor_validate(super->main);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle_validate(self->net_bundles[i]);
  }
}

static void FederatedEnvironment_assemble(Environment *super) {
  FederatedEnvironment *self = (FederatedEnvironment *)super;
  // Here we enter a critical section which do not leave.
  // The scheduler will leave the critical section before executing the reactions.
  // Everything else within the runtime happens in a critical section.
  validaten(super->main->calculate_levels(super->main));
  lf_ret_t ret;
  FederatedEnvironment_validate(super);

  // Establish connections to all neighbors:
  ret = self->startup_coordinator->connect_to_neighbors_blocking(self->startup_coordinator);
  validate(ret == LF_OK);
  self->startup_coordinator->start(self->startup_coordinator);
}

static void FederatedEnvironment_start(Environment *super) {
  // We do not set the start time here in federated mode, instead the StartupCoordinator will do it.
  // So we just start the main loop and the StartupCoordinator.
  super->scheduler->run(super->scheduler);
}

static lf_ret_t FederatedEnvironment_wait_until_locked(Environment *super, instant_t wakeup_time) {
  FederatedEnvironment *self = (FederatedEnvironment *)super;
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

static interval_t FederatedEnvironment_get_physical_time(Environment *super) {
  FederatedEnvironment *self = (FederatedEnvironment *)super;
  return self->clock.get_time(&self->clock);
}

/**
 * @brief Acquire a tag by iterating through all network input ports and making
 * sure that they are resolved at this tag. If the input port is unresolved we
 * must wait for the max_wait time before proceeding.
 *
 * @param self
 * @param next_tag
 * @return lf_ret_t
 */
static lf_ret_t FederatedEnvironment_acquire_tag_locked(Environment *super, tag_t next_tag) {
  LF_DEBUG(SCHED, "Acquiring tag " PRINTF_TAG, next_tag);
  FederatedEnvironment *self = (FederatedEnvironment *)super;
  instant_t additional_sleep = 0;
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle *bundle = self->net_bundles[i];

    if (!bundle->net_channel->is_connected(bundle->net_channel)) {
      continue;
    }

    for (size_t j = 0; j < bundle->inputs_size; j++) {
      FederatedInputConnection *input = bundle->inputs[j];
      // Find the max safe-to-assume-absent value and go to sleep waiting for this.
      if (lf_tag_compare(input->last_known_tag, next_tag) < 0) {
        LF_DEBUG(SCHED, "Input %p is unresolved, latest known tag was " PRINTF_TAG, input, input->last_known_tag);
        LF_DEBUG(SCHED, "Input %p has maxwait of  " PRINTF_TIME, input, input->max_wait);
        if (input->max_wait > additional_sleep) {
          additional_sleep = input->max_wait;
        }
      }
    }
  }

  if (additional_sleep > 0) {
    LF_DEBUG(SCHED, "Need to sleep for additional " PRINTF_TIME " ns", additional_sleep);
    instant_t sleep_until = lf_time_add(next_tag.time, additional_sleep);
    return super->wait_until_locked(super, sleep_until);
  } else {
    return LF_OK;
  }
}

static lf_ret_t FederatedEnvironment_poll_network_channels(Environment *super) {
  FederatedEnvironment *self = (FederatedEnvironment *)super;
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    if (self->net_bundles[i]->net_channel->mode == NETWORK_CHANNEL_MODE_POLLED) {
      PolledNetworkChannel *poll_channel = (PolledNetworkChannel *)self->net_bundles[i]->net_channel;
      poll_channel->poll(poll_channel);
    }
  }
  return LF_OK;
}

void FederatedEnvironment_ctor(FederatedEnvironment *self, Reactor *main, Scheduler *scheduler, bool fast_mode,
                               FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                               StartupCoordinator *startup_coordinator, ClockSynchronization *clock_sync) {
  Environment_ctor(&self->super, main, scheduler, fast_mode);
  self->super.assemble = FederatedEnvironment_assemble;
  self->super.start = FederatedEnvironment_start;
  self->super.wait_until_locked = FederatedEnvironment_wait_until_locked;
  self->super.get_physical_time = FederatedEnvironment_get_physical_time;
  self->super.acquire_tag_locked = FederatedEnvironment_acquire_tag_locked;
  self->super.poll_network_channels = FederatedEnvironment_poll_network_channels;
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

void FederatedEnvironment_free(FederatedEnvironment *self) {
  LF_INFO(ENV, "Reactor shutting down, freeing federated environment.");
  Environment_free(&self->super);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    NetworkChannel *chan = self->net_bundles[i]->net_channel;
    chan->free(chan);
  }
  LF_INFO(ENV, "All Network Channels freed. Exiting.");
}