#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H

#include "reactor-uc/error.h"
#include "reactor-uc/queues.h"

typedef struct Scheduler Scheduler;
typedef struct Environment Environment;

struct Scheduler {
  Environment *env;
  EventQueue event_queue;
  ReactionQueue reaction_queue;
  // The following two fields are used to implement a linked list of Triggers
  // that are registered for cleanup at the end of the current tag.
  Trigger *cleanup_ll_head;
  Trigger *cleanup_ll_tail;
  instant_t start_time; // The physical time at which the program started.
  tag_t stop_tag;       // The tag at which the program should stop. This is set by the user or by the scheduler.
  tag_t current_tag;    // The current logical tag. Set by the scheduler and read by user in the reaction bodies.
  bool keep_alive;      // Whether the program should keep running even if there are no more events to process.

  /**
   * @brief Schedules an event on trigger at a specified tag. This function will
   * enter a critcal section if the environment has async events.
   */
  lf_ret_t (*schedule_at)(Scheduler *self, Event *event);

  /**
   * @brief Schedules an event on a trigger at a specified tag. This function
   * assumes that we are in a critical section (if this is needed).
   *
   */
  lf_ret_t (*schedule_at_locked)(Scheduler *self, Event *event);

  /**
   * @brief Runs the program. Does not return until program has completed.
   */
  void (*run)(Scheduler *self);

  /**
   * @brief After committing to a tag, but before executing reactions, the
   * scheduler must prepare the timestep by adding reactions to the reaction
   * queue.
   */
  void (*prepare_timestep)(Scheduler *self, tag_t tag);

  /**
   * @brief After completing all reactions at a tag, this function is called to
   * reset is_present fields and increment index pointers of the EventPayloadPool.
   */
  void (*clean_up_timestep)(Scheduler *self);

  /**
   * @brief Called after `prepare_timestep` to run all reactions on the current
   * tag.
   */
  void (*run_timestep)(Scheduler *self);

  /**
   * @brief Called to execute all reactions triggered by a shutdown trigger.
   */
  void (*terminate)(Scheduler *self);

  /**
   * @brief Set the stop tag of the program based on a timeout duration.
   */
  void (*set_timeout)(Scheduler *self, interval_t duration);

  /**
   * @brief Register Trigger for cleanup. The cleanup function of the trigger
   * will be called in `clean_up_timestep`.
   */
  void (*register_for_cleanup)(Scheduler *self, Trigger *trigger);
};

void Scheduler_ctor(Scheduler *self, Environment *env);
#endif
