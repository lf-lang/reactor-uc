#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H

#include "reactor-uc/queues.h"

typedef struct Scheduler Scheduler;
typedef struct Environment Environment;

struct Scheduler {
  Environment *env;
  EventQueue event_queue;
  ReactionQueue reaction_queue;
  void (*run)(Scheduler *self);
  void (*prepare_timestep)(Scheduler *self);
  void (*clean_up_timestep)(Scheduler *self);
};

void Scheduler_ctor(Scheduler *self, Environment *env);
#endif
