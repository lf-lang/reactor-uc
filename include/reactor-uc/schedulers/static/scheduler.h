//
// Created by tanneberger on 11/16/24.
//

#ifndef STATIC_SCHEDULER_H
#define STATIC_SCHEDULER_H
#include "reactor-uc/error.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/static/scheduler_instructions.h"

typedef struct StaticScheduler StaticScheduler;
typedef struct Environment Environment;

struct StaticScheduler {
  Scheduler super;
  Environment *env;
  const inst_t **static_schedule;
  size_t *pc;
};

void StaticScheduler_ctor(StaticScheduler *self, Environment *env, const inst_t **static_schedule);

#endif // STATIC_SCHEDULER_H
