//
// Created by tanneberger on 11/16/24.
//

#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "reactor-uc/error.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/scheduler.h"

typedef struct DynamicScheduler DynamicScheduler;
typedef struct Environment Environment;

struct DynamicScheduler {
  Scheduler scheduler;
  Environment *env;
  EventQueue event_queue;
  ReactionQueue reaction_queue;
  // The following two fields are used to implement a linked list of Triggers
  // that are registered for cleanup at the end of the current tag.
  Trigger *cleanup_ll_head;
  Trigger *cleanup_ll_tail;
  bool leader; // Whether this scheduler is the leader in a federated program and selects the start tag.
  // instant_t start_time; // The physical time at which the program started.
  tag_t stop_tag;    // The tag at which the program should stop. This is set by the user or by the scheduler.
  tag_t current_tag; // The current logical tag. Set by the scheduler and read by user in the reaction bodies.
};

void DynamicScheduler_ctor(DynamicScheduler *self, Environment *env);

#endif // SCHEDULER_H
