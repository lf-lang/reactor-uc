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
#include <reactor-uc/encryption_layer.h>

void Environment_validate(Environment *self) {
  Reactor_validate(self->main);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle_validate(self->net_bundles[i]);
  }
}

void Environment_assemble(Environment *self) {
  validaten(self->main->calculate_levels(self->main));
  lf_ret_t ret;
  Environment_validate(self);

  if (self->is_federated) {
    ret = self->startup_coordinator->connect_to_neigbors(self->startup_coordinator);
    validate(ret == LF_OK);
    ret = self->startup_coordinator->perform_handshake(self->startup_coordinator);
    validate(ret == LF_OK);
  }
}

void Environment_start(Environment *self) {
  instant_t start_time;
  if (self->is_federated) {
    start_time = self->startup_coordinator->negotiate_start_time(self->startup_coordinator);
  } else {
    start_time = self->get_physical_time(self);
  }
  self->scheduler->set_and_schedule_start_tag(self->scheduler, start_time);
  self->scheduler->run(self->scheduler);
}

lf_ret_t Environment_wait_until(Environment *self, instant_t wakeup_time) {
  if (wakeup_time <= self->get_physical_time(self) || self->fast_mode) {
    return LF_OK;
  }

  if (self->has_async_events) {
    return self->platform->wait_until_interruptible(self->platform, wakeup_time);
  } else {
    return self->platform->wait_until(self->platform, wakeup_time);
  }
}

interval_t Environment_get_logical_time(Environment *self) {
  return self->scheduler->current_tag(self->scheduler).time;
}
interval_t Environment_get_elapsed_logical_time(Environment *self) {
  return self->scheduler->current_tag(self->scheduler).time - self->scheduler->start_time;
}
interval_t Environment_get_physical_time(Environment *self) {
  return self->platform->get_physical_time(self->platform);
}
interval_t Environment_get_elapsed_physical_time(Environment *self) {
  if (self->scheduler->start_time == NEVER) {
    return 0;
  } else {
    return self->platform->get_physical_time(self->platform) - self->scheduler->start_time;
  }
}
void Environment_enter_critical_section(Environment *self) {
  if (self->has_async_events) {
    self->platform->enter_critical_section(self->platform);
  }
}
void Environment_leave_critical_section(Environment *self) {
  if (self->has_async_events) {
    self->platform->leave_critical_section(self->platform);
  }
}

void Environment_request_shutdown(Environment *self) { self->scheduler->request_shutdown(self->scheduler); }

void Environment_ctor(Environment *self, Reactor *main, interval_t duration, EventQueue *event_queue,
                      EventQueue *system_event_queue, ReactionQueue *reaction_queue, bool keep_alive, bool is_federated,
                      bool fast_mode, FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                      StartupCoordinator *startup_coordinator) {
  self->main = main;
  self->scheduler = Scheduler_new(self, event_queue, system_event_queue, reaction_queue, duration, keep_alive);
  self->platform = Platform_new();
  Platform_ctor(self->platform);
  self->platform->initialize(self->platform);
  self->assemble = Environment_assemble;
  self->start = Environment_start;
  self->wait_until = Environment_wait_until;
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

  if (self->is_federated) {
    validate(self->net_bundles);
    validate(self->startup_coordinator);
    self->has_async_events = true;
  }

  self->startup = NULL;
  self->shutdown = NULL;
}

void Environment_free(Environment *self) {
  (void)self;
  LF_INFO(ENV, "Reactor shutting down, freeing environment.");
  self->leave_critical_section(self);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    NetworkChannel *chan = self->net_bundles[i]->encryption_layer->network_channel;
    chan->free(chan);
  }
}