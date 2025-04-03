#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"
#include <assert.h>
#include <inttypes.h>

static void Environment_validate(Environment *self) {
  Reactor_validate(self->main);
}

static void Environment_assemble(Environment *self) {
  // Here we enter a critical section which do not leave.
  // The scheduler will leave the critical section before executing the reactions.
  // Everything else within the runtime happens in a critical section.
  self->enter_critical_section(self);
  validaten(self->main->calculate_levels(self->main));
  Environment_validate(self);
}

static void Environment_start(Environment *self) {
  instant_t start_time = self->get_physical_time(self);
  self->scheduler->set_and_schedule_start_tag(self->scheduler, start_time);
  self->scheduler->run(self->scheduler);
}

static lf_ret_t Environment_wait_until_locked(Environment *self, instant_t wakeup_time) {
  if (wakeup_time <= self->get_physical_time(self) || self->fast_mode) {
    return LF_OK;
  }

  if (self->has_async_events) {
    return self->platform->wait_until_interruptible_locked(self->platform, wakeup_time);
  } else {
    return self->platform->wait_until(self->platform, wakeup_time);
  }
}

static interval_t Environment_get_logical_time(Environment *self) {
  return self->scheduler->current_tag(self->scheduler).time;
}
static interval_t Environment_get_elapsed_logical_time(Environment *self) {
  return self->scheduler->current_tag(self->scheduler).time - self->scheduler->start_time;
}
static interval_t Environment_get_physical_time(Environment *self) {
  return self->platform->get_physical_time(self->platform);
}
static interval_t Environment_get_elapsed_physical_time(Environment *self) {
  if (self->scheduler->start_time == NEVER) {
    return NEVER;
  } else {
    return self->platform->get_physical_time(self->platform) - self->scheduler->start_time;
  }
}
static void Environment_enter_critical_section(Environment *self) {
  if (self->has_async_events) {
    self->platform->enter_critical_section(self->platform);
  }
}
static void Environment_leave_critical_section(Environment *self) {
  if (self->has_async_events) {
    self->platform->leave_critical_section(self->platform);
  }
}

static void Environment_request_shutdown(Environment *self) {
  self->scheduler->request_shutdown(self->scheduler);
}

void Environment_ctor(Environment *self, Reactor *main, Scheduler *scheduler, bool fast_mode) {
  self->main = main;
  self->scheduler = scheduler;
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
  self->acquire_tag = NULL;
  self->poll_network_channels = NULL;
  self->has_async_events = false; // Will be overwritten if a physical action is registered
  self->fast_mode = fast_mode;

  self->startup = NULL;
  self->shutdown = NULL;
}

void Environment_free(Environment *self) {
  (void)self;
  LF_INFO(ENV, "Reactor shutting down, freeing environment.");
}