#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>
#include <inttypes.h>

void Environment_assemble(Environment *self) { validaten(self->main->calculate_levels(self->main)); }

// Find the start time of the federation.
// FIXME: This needs to involve communcation with the other federates. Currently hardcoded to 1 second.
void Environment_set_start_time(Environment *self) {
#if defined(PLATFORM_POSIX)
  self->start_time = ((self->platform->get_physical_time(self->platform) + SEC(1)) / SEC(1)) * SEC(1);
#elif defined(PLATFORM_RIOT)
  self->start_time = SEC(1);
#elif defined(PLATFORM_ZEPHY)
  self->start_time = SEC(1);
#endif
  LF_INFO(ENV, "Start time: %" PRId64, self->start_time);
}

void Environment_start(Environment *self) {
  Environment_set_start_time(self);
  self->scheduler.run(&self->scheduler);
}

lf_ret_t Environment_wait_until(Environment *self, instant_t wakeup_time) {
  if (wakeup_time < self->get_physical_time(self)) {
    return LF_OK;
  }

  if (self->has_async_events) {
    return self->platform->wait_until_interruptible(self->platform, wakeup_time);
  } else {
    return self->platform->wait_until(self->platform, wakeup_time);
  }
}

interval_t Environment_get_logical_time(Environment *self) { return self->scheduler.current_tag.time; }
interval_t Environment_get_elapsed_logical_time(Environment *self) {
  return self->scheduler.current_tag.time - self->scheduler.start_time;
}
interval_t Environment_get_physical_time(Environment *self) {
  return self->platform->get_physical_time(self->platform);
}
interval_t Environment_get_elapsed_physical_time(Environment *self) {
  return self->platform->get_physical_time(self->platform) - self->scheduler.start_time;
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

void Environment_ctor(Environment *self, Reactor *main) {
  self->main = main;
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
  self->has_async_events = false;
  self->startup = NULL;
  self->shutdown = NULL;
  Scheduler_ctor(&self->scheduler, self);
}

void Environment_free(Environment *self) {
  (void)self;
  LF_INFO(ENV, "Freeing environment");
  for (size_t i = 0; i < self->bundles_size; i++) {
    NetworkChannel *chan = self->bundles[i]->net_channel;
    chan->free(chan);
  }
}