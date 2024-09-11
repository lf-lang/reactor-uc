#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

typedef struct Environment Environment;

struct Environment {
  Reactor *main;
  Scheduler scheduler;
  tag_t current_tag;
  bool keep_alive;
  bool running;
  void (*assemble)(Environment *self);
  void (*start)(Environment *self);
  int (*wait_until)(Environment *self, instant_t wakeup_time);
  void (*calculate_levels)(Environment *self);
};

void Environment_ctor(Environment *self, Reactor *main);
void Environment_assemble(Environment *self);
void Environment_start(Environment *self);
void Environment_calculate_levels(Environment *self);

#endif
