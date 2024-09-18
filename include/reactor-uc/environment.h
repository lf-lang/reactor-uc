#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

extern Environment lf_global_env;

typedef struct Environment Environment;

struct Environment {
  Reactor *main;
  Scheduler scheduler;
  tag_t stop_tag;
  tag_t current_tag;
  bool keep_alive;
  Startup *startup;
  Shutdown *shutdown;
  void (*assemble)(Environment *self);
  void (*start)(Environment *self);
  int (*wait_until)(Environment *self, instant_t wakeup_time);
  void (*terminate)(Environment *self);
};

void Environment_ctor(Environment *self, Reactor *main);

#endif
