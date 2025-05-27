#include "reactor-uc/environments/federate_environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/startup_coordinator.h"
#include <assert.h>
#include <inttypes.h>

static void FederateEnvironment_validate(Environment *super) {
  FederateEnvironment *self = (FederateEnvironment *)super;
  Reactor_validate(super->main);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle_validate(self->net_bundles[i]);
  }
}

static void FederateEnvironment_assemble(Environment *super) {
  FederateEnvironment *self = (FederateEnvironment *)super;
  // Here we enter a critical section which do not leave.
  // The scheduler will leave the critical section before executing the reactions.
  // Everything else within the runtime happens in a critical section.
  validaten(super->main->calculate_levels(super->main));
  lf_ret_t ret;
  FederateEnvironment_validate(super);

  // Establish connections to all neighbors:
  ret = self->startup_coordinator->connect_to_neighbors_blocking(self->startup_coordinator);
  validate(ret == LF_OK);
  self->startup_coordinator->start(self->startup_coordinator);
}

static void FederateEnvironment_start(Environment *super) {
  // We do not set the start time here in federated mode, instead the StartupCoordinator will do it.
  // So we just start the main loop and the StartupCoordinator.
  super->scheduler->run(super->scheduler);
}

static lf_ret_t FederateEnvironment_wait_until(Environment *super, instant_t wakeup_time) {
  FederateEnvironment *self = (FederateEnvironment *)super;
  if (wakeup_time <= super->get_physical_time(super) || super->fast_mode) {
    return LF_OK;
  }

  // Translate the wakeup time into the time scale used by the platform.
  // This is different from the application-level time scale if we are running
  // distributed with clock-sync enabled.
  instant_t hw_wakeup_time = self->clock.to_hw_time(&self->clock, wakeup_time);

  if (super->has_async_events) {
    return super->platform->wait_until_interruptible(super->platform, hw_wakeup_time);
  } else {
    return super->platform->wait_until(super->platform, hw_wakeup_time);
  }
}

static interval_t FederateEnvironment_get_physical_time(Environment *super) {
  FederateEnvironment *self = (FederateEnvironment *)super;
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
static lf_ret_t FederateEnvironment_acquire_tag(Environment *super, tag_t next_tag) {
  LF_DEBUG(SCHED, "Acquiring tag " PRINTF_TAG, next_tag);
  FederateEnvironment *self = (FederateEnvironment *)super;
  instant_t additional_sleep = 0;
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle *bundle = self->net_bundles[i];

    if (!bundle->net_channel->is_connected(bundle->net_channel)) {
      continue;
    }

    for (size_t j = 0; j < bundle->inputs_size; j++) {
      FederatedInputConnection *input = bundle->inputs[j];
      if (input->type == PHYSICAL_CONNECTION)
        continue;
      // Before reading the last_known_tag of an FederatedInputConnection, we must acquire its mutex.
      MUTEX_LOCK(input->mutex);
      if (lf_tag_compare(input->last_known_tag, next_tag) < 0) {
        LF_DEBUG(SCHED, "Input %p is unresolved, latest known tag was " PRINTF_TAG, input, input->last_known_tag);
        LF_DEBUG(SCHED, "Input %p has maxwait of  " PRINTF_TIME, input, input->max_wait);
        if (input->max_wait > additional_sleep) {
          additional_sleep = input->max_wait;
        }
      }
      MUTEX_UNLOCK(input->mutex);
    }
  }

  if (additional_sleep > 0) {
    LF_DEBUG(SCHED, "Need to sleep for additional " PRINTF_TIME " ns", additional_sleep);
    instant_t sleep_until = lf_time_add(next_tag.time, additional_sleep);
    return super->wait_until(super, sleep_until);
  } else {
    return LF_OK;
  }
}

static lf_ret_t FederateEnvironment_poll_network_channels(Environment *super) {
  FederateEnvironment *self = (FederateEnvironment *)super;
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    if (self->net_bundles[i]->net_channel->mode == NETWORK_CHANNEL_MODE_POLLED) {
      PolledNetworkChannel *poll_channel = (PolledNetworkChannel *)self->net_bundles[i]->net_channel;
      poll_channel->poll(poll_channel);
    }
  }
  return LF_OK;
}

void FederateEnvironment_ctor(FederateEnvironment *self, Reactor *main, Scheduler *scheduler, bool fast_mode,
                              FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                              StartupCoordinator *startup_coordinator, ClockSynchronization *clock_sync) {
  Environment_ctor(&self->super, ENVIRONMENT_FEDERATE, main, scheduler, fast_mode);
  self->super.assemble = FederateEnvironment_assemble;
  self->super.start = FederateEnvironment_start;
  self->super.wait_until = FederateEnvironment_wait_until;
  self->super.get_physical_time = FederateEnvironment_get_physical_time;
  self->super.acquire_tag = FederateEnvironment_acquire_tag;
  self->super.poll_network_channels = FederateEnvironment_poll_network_channels;
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

void FederateEnvironment_free(FederateEnvironment *self) {
  LF_INFO(ENV, "Reactor shutting down, freeing federated environment.");
  Environment_free(&self->super);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    NetworkChannel *chan = self->net_bundles[i]->net_channel;
    chan->free(chan);
  }
  LF_INFO(ENV, "All Network Channels freed. Exiting.");
}