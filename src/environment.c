#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>
#include <inttypes.h>

void Environment_validate(Environment *self) {
  Reactor_validate(self->main);
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    FederatedConnectionBundle_validate(self->net_bundles[i]);
  }
}

void Environment_assemble(Environment *self) {
  validaten(self->main->calculate_levels(self->main));
  Environment_validate(self);
  if (self->net_bundles_size > 0) {
    FederatedConnectionBundle_connect_to_peers(self->net_bundles, self->net_bundles_size);
  }
}

void Environment_start(Environment *self) {
  self->scheduler->acquire_and_schedule_start_tag(self->scheduler);
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
  return self->platform->get_physical_time(self->platform) - self->scheduler->start_time;
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

void Environment_ctor(Environment *self, Reactor *main) {
  self->main = main;
  self->scheduler = Scheduler_new(self);
  self->platform = Platform_new();
  Platform_ctor(self->platform);
  self->platform->initialize(self->platform);
  self->net_bundles_size = 0;
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
  self->has_async_events = false;
  self->fast_mode = false;
  self->startup = NULL;
  self->shutdown = NULL;
}

void Environment_free(Environment *self) {
  (void)self;
  LF_INFO(ENV, "Freeing environment");
  for (size_t i = 0; i < self->net_bundles_size; i++) {
    NetworkChannel *chan = self->net_bundles[i]->net_channel;
    chan->free(chan);
  }
}