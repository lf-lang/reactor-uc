#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"
#include <assert.h>
#include <inttypes.h>

static void *enclave_thread(void *environment_pointer) {
  Environment *env = (Environment *)environment_pointer;
  env->start(env);
  return NULL;
}

static void Environment_validate(Environment *self) {
  Reactor_validate(self->main);
}

static void Environment_assemble(Environment *self) {
  validaten(self->main->calculate_levels(self->main));
  Environment_validate(self);
}

static void Environment_start(Environment *self) {
  instant_t start_time = self->get_physical_time(self);
  LF_INFO(ENV, "Starting program at " PRINTF_TIME " nsec", start_time);
  for (size_t i = 0; i < self->num_enclaved_environments; i++) {
    self->platform->create_thread(self->platform, self->enclaved_environments[i]->thread, enclave_thread,
                                  (void *)self->enclaved_environments[i]);
  }
  self->scheduler->set_and_schedule_start_tag(self->scheduler, start_time);
  self->scheduler->run(self->scheduler);
}

static lf_ret_t Environment_wait_until(Environment *self, instant_t wakeup_time) {
  if (wakeup_time <= self->get_physical_time(self) || self->fast_mode) {
    return LF_OK;
  }

  if (self->has_async_events) {
    return self->platform->wait_until_interruptible(self->platform, wakeup_time);
  } else {
    return self->platform->wait_until(self->platform, wakeup_time);
  }
}

static lf_ret_t Environment_wait_for(Environment *self, interval_t wait_time) {
  if (wait_time <= 0 || self->fast_mode) {
    return LF_OK;
  }

  return self->platform->wait_for(self->platform, wait_time);
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

static void Environment_request_shutdown(Environment *self) {
  self->scheduler->request_shutdown(self->scheduler);
}

static interval_t Environment_get_lag(Environment *self) {
  return self->get_physical_time(self) - self->get_logical_time(self);
}

void Environment_ctor(Environment *self, Reactor *main, Scheduler *scheduler, bool fast_mode) {
  self->main = main;
  self->scheduler = scheduler;
  self->platform = Platform_new();
  Platform_ctor(self->platform);
  self->assemble = Environment_assemble;
  self->start = Environment_start;
  self->wait_until = Environment_wait_until;
  self->get_elapsed_logical_time = Environment_get_elapsed_logical_time;
  self->get_logical_time = Environment_get_logical_time;
  self->get_physical_time = Environment_get_physical_time;
  self->get_elapsed_physical_time = Environment_get_elapsed_physical_time;
  self->request_shutdown = Environment_request_shutdown;
  self->wait_for = Environment_wait_for;
  self->get_lag = Environment_get_lag;
  self->acquire_tag = NULL;
  self->poll_network_channels = NULL;
  self->has_async_events = false; // Will be overwritten if a physical action is registered
  self->fast_mode = fast_mode;

  self->startup = NULL;
  self->shutdown = NULL;
}

void Environment_free(Environment *self) {
  (void)self;
  LF_DEBUG(ENV, "Freeing top-level environment.");
}
