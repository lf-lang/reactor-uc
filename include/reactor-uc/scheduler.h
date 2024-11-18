#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H

#include "reactor-uc/error.h"
#include "reactor-uc/queues.h"

typedef struct Scheduler Scheduler;
typedef struct Environment Environment;

struct Scheduler {
 long int start_time;

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
  void (*do_shutdown)(Scheduler *self, tag_t stop_tag);

  void (*request_shutdown)(Scheduler *self);

  /**
   * @brief Register Trigger for cleanup. The cleanup function of the trigger
   * will be called in `clean_up_timestep`.
   */
  void (*register_for_cleanup)(Scheduler *self, Trigger *trigger);

  void (*acquire_and_schedule_start_tag)(Scheduler *self);

  void (*set_duration)(Scheduler* self, interval_t duration);

  lf_ret_t (*add_to_reaction_queue)(Scheduler *self, Reaction* reaction);

  tag_t (*current_tag)(Scheduler* self);
};

void Scheduler_ctor(Scheduler* self);

#endif
