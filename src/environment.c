#include "reactor-uc/environment.h"
#include "reactor-uc/hardware_interface.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

#include <stdio.h>

void Environment_assemble(Environment *self) {
  (void)self;
  printf("Assembling environment\n");
}
void Environment_start(Environment *self) {
  (void)self;
  printf("Running program\n");

  self->scheduler.run(&self->scheduler);
}

int Environment_wait_until(Environment *self, instant_t wakeup_time) {
  hardware_wait_for(self->current_tag.time, wakeup_time);
  return 0;
}

void Environment_ctor(Environment *self, Reactor *main) {
  self->main = main;
  self->assemble = Environment_assemble;
  self->start = Environment_start;
  self->keep_alive = false;
  self->wait_until = Environment_wait_until;
  Scheduler_ctor(&self->scheduler, self);
}
