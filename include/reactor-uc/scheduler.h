#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H


typedef struct Scheduler Scheduler;
typedef struct Environment Environment;
typedef struct EventQueue EventQueue;
typedef struct ReactionQueue ReactionQueue;

#include "reactor-uc/queues.h"

struct Scheduler {
  // methods
  void (*run)(Scheduler *self);
  void (*prepare_time_step)(Scheduler *self);

  // member variables
  Environment *env_;
  EventQueue event_queue_;
  ReactionQueue reaction_queue_;
};

void Scheduler_ctor(Scheduler *self, Environment *env);

#endif
