#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

typedef struct Environment Environment;

struct Environment {
  Reactor **reactors;
  size_t reactors_size;
  Scheduler scheduler;
  tag_t current_tag;
  bool keep_alive;
  void (*assemble)(Environment *self);
  void (*start)(Environment *self);
  int (*wait_until)(Environment *self, instant_t wakeup_time);
};

void Environment_ctor(Environment *self, Reactor **reactors, size_t reactors_size);
void Environment_assemble(Environment *self);
void Environment_start(Environment *self);

#endif
