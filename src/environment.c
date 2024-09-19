#include "reactor-uc/environment.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

#include <assert.h>
#include <stdio.h>

void Environment_assemble(Environment *self) {
  printf("Assembling environment\n");

  printf("Assigning levels\n");
  self->main->calculate_levels(self->main);
}

void Environment_start(Environment *self) {
  (void)self;

  printf("Running program\n");

  self->scheduler.run(&self->scheduler);
}

int Environment_wait_until(Environment *self, instant_t wakeup_time) {
  self->platform->wait_until(self->platform, wakeup_time);
  return 0;
}

void Environment_set_stop_time(Environment *self, interval_t duration) {
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
  self->set_stop_time = Environment_set_stop_time;
  self->get_elapsed_logical_time = Environment_get_elapsed_logical_time;
  self->get_logical_time = Environment_get_logical_time;
  self->get_physical_time = Environment_get_physical_time;
  self->get_elapsed_physical_time = Environment_get_elapsed_physical_time;

  self->keep_alive = false;
  self->startup = NULL;
  self->shutdown = NULL;
  self->stop_tag = FOREVER_TAG;
  Scheduler_ctor(&self->scheduler, self);
  self->current_tag.microstep = 0;
  self->current_tag.time = 0;
  self->start_time = NEVER;

  // Set start time
  self->start_time = self->platform->get_physical_time(self->platform);
  self->current_tag.time = self->start_time;
}
