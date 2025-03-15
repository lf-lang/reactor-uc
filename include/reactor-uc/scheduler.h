#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H

#include "reactor-uc/reactor-uc.h"

typedef struct Scheduler Scheduler;
typedef struct Environment Environment;

struct Scheduler {
  instant_t start_time;
  interval_t duration; // The duration after which the program should stop.
  bool keep_alive;     // Whether the program should keep running even if there are no more events to process.
  bool leader;         // Whether this scheduler is the leader in a federated program and selects the start tag.

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

  // void (*set_duration)(Scheduler *self, interval_t duration);

  lf_ret_t (*add_to_reaction_queue)(Scheduler *self, Reaction *reaction);

  tag_t (*current_tag)(Scheduler *self);
};

Scheduler *Scheduler_new(Environment *env);

#if defined(SCHEDULER_DYNAMIC)
#include "schedulers/dynamic/scheduler.h"
#elif defined(SCHEDULER_STATIC)
#include "schedulers/static/scheduler.h"
#else
#error "Scheduler not supported"
#endif
#endif // REACTOR_UC_SCHEDULER_H