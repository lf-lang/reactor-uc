#include "reactor-uc/environment.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

#include <assert.h>

void Environment_assemble(Environment *self) {
  printf("Assembling environment\n");
  validaten(self->main->calculate_levels(self->main));
}

void Environment_start(Environment *self) {
  printf("Running program\n");
  self->scheduler.run(&self->scheduler);
}

lf_ret_t Environment_wait_until(Environment *self, instant_t wakeup_time) {
  if (wakeup_time < self->get_physical_time(self)) {
    return LF_OK;
  }

  if (self->has_physical_action) {
    return self->platform->wait_until_interruptable(self->platform, wakeup_time);
  } else {
    return self->platform->wait_until(self->platform, wakeup_time);
  }
}

void Environment_set_timeout(Environment *self, interval_t duration) {
  self->stop_tag.microstep = 0;
  self->stop_tag.time = self->start_time + duration;
}

interval_t Environment_get_logical_time(Environment *self) { return self->current_tag.time; }
interval_t Environment_get_elapsed_logical_time(Environment *self) { return self->current_tag.time - self->start_time; }
interval_t Environment_get_physical_time(Environment *self) {
  return self->platform->get_physical_time(self->platform);
}
interval_t Environment_get_elapsed_physical_time(Environment *self) {
  return self->platform->get_physical_time(self->platform) - self->start_time;
}

void Environment_ctor(Environment *self, Reactor *main) {
  self->main = main;
  self->platform = Platform_new();
  Platform_ctor(self->platform);

  self->assemble = Environment_assemble;
  self->start = Environment_start;
  self->wait_until = Environment_wait_until;
  self->set_timeout = Environment_set_timeout;
  self->get_elapsed_logical_time = Environment_get_elapsed_logical_time;
  self->get_logical_time = Environment_get_logical_time;
  self->get_physical_time = Environment_get_physical_time;
  self->get_elapsed_physical_time = Environment_get_elapsed_physical_time;

  self->keep_alive = false;
  self->has_physical_action = false;
  self->startup = NULL;
  self->shutdown = NULL;
  self->stop_tag = FOREVER_TAG;
  Scheduler_ctor(&self->scheduler, self);
  self->current_tag = NEVER_TAG;
  self->start_time = NEVER;

  // Set start time
  self->start_time = self->platform->get_physical_time(self->platform);
}
