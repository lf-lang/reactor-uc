#ifndef SCHEDULER_FEDERATED_H
#define SCHEDULER_FEDERATED_H
#include "reactor-uc/queues.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"

typedef struct DynamicSchedulerFederated DynamicSchedulerFederated;
typedef struct EnvironmentFederated EnvironmentFederated;

struct DynamicSchedulerFederated {
  DynamicScheduler super;
};

void DynamicSchedulerFederated_ctor(DynamicSchedulerFederated *self, EnvironmentFederated *env, EventQueue *event_queue,
                                    EventQueue *system_event_queue, ReactionQueue *reaction_queue, interval_t duration,
                                    bool keep_alive);

#endif // SCHEDULER_H
