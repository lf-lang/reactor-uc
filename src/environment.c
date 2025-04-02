#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/startup_coordinator.h"
#include <assert.h>
#include <inttypes.h>

void Environment_validate(Environment *self) {
  Reactor_validate(self->main);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle_validate(self->net_bundles[i]);
  }
}

void Environment_assemble(Environment *self) {
  // Here we enter a critical section which do not leave.
  // The scheduler will leave the critical section before executing the reactions.
  // Everything else within the runtime happens in a critical section.
  self->enter_critical_section(self);
  validaten(self->main->calculate_levels(self->main));
  lf_ret_t ret;
  Environment_validate(self);

  // Establish connections to all neighbors:
  if (self->is_federated) {
    ret = self->startup_coordinator->connect_to_neighbors_blocking(self->startup_coordinator);
    validate(ret == LF_OK);
    self->startup_coordinator->start(self->startup_coordinator);
  }
}

void Environment_start(Environment *self) {
  instant_t start_time;
  if (!self->is_federated) {
    start_time = self->get_physical_time(self);
    self->scheduler->set_and_schedule_start_tag(self->scheduler, start_time);
  }
  self->scheduler->run(self->scheduler);
}

lf_ret_t Environment_wait_until_locked(Environment *self, instant_t wakeup_time) {
  if (wakeup_time <= self->get_physical_time(self) || self->fast_mode) {
    return LF_OK;
  }

  // Translate the wakeup time into the time scale used by the platform.
  // This is different from the application-level time scale if we are running
  // distributed with clock-sync enabled.
  instant_t hw_wakeup_time = self->clock.to_hw_time(&self->clock, wakeup_time);

  if (self->has_async_events) {
    return self->platform->wait_until_interruptible_locked(self->platform, hw_wakeup_time);
  } else {
    return self->platform->wait_until(self->platform, hw_wakeup_time);
  }
}

interval_t Environment_get_logical_time(Environment *self) {
  return self->scheduler->current_tag(self->scheduler).time;
}
interval_t Environment_get_elapsed_logical_time(Environment *self) {
  return self->scheduler->current_tag(self->scheduler).time - self->scheduler->start_time;
}
interval_t Environment_get_physical_time(Environment *self) {
  return self->clock.get_time(&self->clock);
}
interval_t Environment_get_elapsed_physical_time(Environment *self) {
  if (self->scheduler->start_time == NEVER) {
    return NEVER;
  } else {
    return self->clock.get_time(&self->clock) - self->scheduler->start_time;
  }
}
void Environment_enter_critical_section(Environment *self) {
  (void)self;
  if (self->has_async_events) {
    self->platform->enter_critical_section(self->platform);
  }
}

void Environment_leave_critical_section(Environment *self) {
  (void)self;
  if (self->has_async_events) {
    self->platform->leave_critical_section(self->platform);
  }
}

void Environment_request_shutdown(Environment *self) {
  self->scheduler->request_shutdown(self->scheduler);
}

void Environment_ctor(Environment *self, Reactor *main, interval_t duration, EventQueue *event_queue,
                      EventQueue *system_event_queue, ReactionQueue *reaction_queue, bool keep_alive, bool is_federated,
                      bool fast_mode, FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                      StartupCoordinator *startup_coordinator, ClockSynchronization *clock_sync) {
  self->main = main;
  self->scheduler = Scheduler_new(self, event_queue, system_event_queue, reaction_queue, duration, keep_alive);
  self->platform = Platform_new();
  Platform_ctor(self->platform);
  self->platform->initialize(self->platform);
  self->assemble = Environment_assemble;
  self->start = Environment_start;
  self->wait_until_locked = Environment_wait_until_locked;
  self->get_elapsed_logical_time = Environment_get_elapsed_logical_time;
  self->get_logical_time = Environment_get_logical_time;
  self->get_physical_time = Environment_get_physical_time;
  self->get_elapsed_physical_time = Environment_get_elapsed_physical_time;
  self->leave_critical_section = Environment_leave_critical_section;
  self->enter_critical_section = Environment_enter_critical_section;
  self->request_shutdown = Environment_request_shutdown;
  self->has_async_events = false; // Will be overwritten if a physical action is registered
  self->fast_mode = fast_mode;
  self->is_federated = is_federated;
  self->net_bundles_size = net_bundles_size;
  self->net_bundles = net_bundles;
  self->startup_coordinator = startup_coordinator;
  self->clock_sync = clock_sync;
  self->do_clock_sync = clock_sync != NULL;
  PhysicalClock_ctor(&self->clock, self, self->do_clock_sync);

  if (self->is_federated) {
    validate(self->net_bundles_size > 0);
    validate(self->net_bundles);
    validate(self->startup_coordinator);
    self->has_async_events = true;
  }

  self->startup = NULL;
  self->shutdown = NULL;
}

void Environment_free(Environment *self) {
  LF_INFO(ENV, "Reactor shutting down, freeing environment.");
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    NetworkChannel *chan = self->net_bundles[i]->net_channel;
    chan->free(chan);
  }
}
