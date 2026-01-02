//
// Created by francesco on 10/01/25.
//

#ifndef PRIORITY_SCHEDULER_H
#define PRIORITY_SCHEDULER_H

#include "reactor-uc/schedulers/dynamic/scheduler.h"

typedef struct PriorityScheduler PriorityScheduler;

struct PriorityScheduler {
  DynamicScheduler super;
};

void PriorityScheduler_ctor(PriorityScheduler *self, Environment *env, EventQueue *event_queue,
                           EventQueue *system_event_queue, ReactionQueue *reaction_queue, interval_t duration,
                           bool keep_alive);

#endif // PRIORITY_SCHEDULER_H
