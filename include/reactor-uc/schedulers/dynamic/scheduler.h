//
// Created by tanneberger on 11/16/24.
//

#ifndef REACTOR_UC_DYNAMIC_SCHEDULER_H
#define REACTOR_UC_DYNAMIC_SCHEDULER_H
#include "reactor-uc/queues.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/platform.h"

typedef struct DynamicScheduler DynamicScheduler;
typedef struct Environment Environment;

struct DynamicScheduler {
  Scheduler super;
  MUTEX_T mutex;
  Environment* env;
  EventQueue* event_queue;
  ReactionQueue* reaction_queue;
  EventQueue* system_event_queue;

  // The following two fields are used to implement a linked list of Triggers
  // that are registered for cleanup at the end of the current tag.
  Trigger* cleanup_ll_head;
  Trigger* cleanup_ll_tail;
  tag_t stop_tag;    // The tag at which the program should stop. This is set by the user or by the scheduler.
  tag_t current_tag; // The current logical tag. Set by the scheduler and read by user in the reaction bodies.

  /**
   * @brief After completing all reactions at a tag, this function is called to
   * reset is_present fields and increment index pointers of the EventPayloadPool.
   */
  void (*clean_up_timestep)(Scheduler* self);

  /**
   * @brief Called after `prepare_timestep` to run all reactions on the current
   * tag.
   */
  void (*run_timestep)(Scheduler* self);
};

void DynamicScheduler_ctor(DynamicScheduler* self, Environment* env, EventQueue* event_queue,
                           EventQueue* system_event_queue, ReactionQueue* reaction_queue, interval_t duration,
                           bool keep_alive);

#endif // REACTOR_UC_DYNAMIC_SCHEDULER_H
