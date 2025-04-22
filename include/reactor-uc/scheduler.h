#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H

#include "reactor-uc/queues.h"
#include "reactor-uc/event.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"

typedef struct Scheduler Scheduler;
typedef struct Environment Environment;

struct Scheduler {
  interval_t start_time; // The logical start time of the program.
  interval_t duration;   // The duration after which the program should stop.
  bool keep_alive;       // Whether the program should keep running even if there are no more events to process.

  /**
   * @brief Schedules an event on trigger at a specified tag. This function will
   * enter a critcal section if the environment has async events.
   */
  lf_ret_t (*schedule_at)(Scheduler *self, Event *event);

  /**
   * @brief Schedules a system event at a specified tag. This function will
   * enter a critcal section if the environment has async events.
   */
  lf_ret_t (*schedule_system_event_at)(Scheduler *self, SystemEvent *event);

  /**
   * @brief Runs the program. Does not return until program has completed.
   */
  void (*run)(Scheduler *self);

  /**
   * @brief This function is called if ClockSynchronization steps the clock.
   * The scheduler should adjust the tag of system_events to make sure they are not lost.
   */
  void (*step_clock)(Scheduler *self, interval_t step);

  /**
   * @brief Called to execute all reactions triggered by a shutdown trigger.
   */
  void (*do_shutdown)(Scheduler *self, tag_t stop_tag);

  /** Request a shutdown from the scheduler. Shutdown will occur at the next available tag. */
  void (*request_shutdown)(Scheduler *self);

  /**
   * @brief Register Trigger for cleanup. The cleanup function of the trigger
   * will be called in `clean_up_timestep`.
   */
  void (*register_for_cleanup)(Scheduler *self, Trigger *trigger);

  /** Set the start time of the program and schedule startup and timer events. */
  void (*set_and_schedule_start_tag)(Scheduler *self, instant_t start_time);

  /** Add a reaction to the reaction queue. */
  lf_ret_t (*add_to_reaction_queue)(Scheduler *self, Reaction *reaction);

  /** Get the current executing tag. */
  tag_t (*current_tag)(Scheduler *self);
};

Scheduler *Scheduler_new(Environment *env, EventQueue *event_queue, EventQueue *system_event_queue,
                         ReactionQueue *reaction_queue, interval_t duration, bool keep_alive);

#if defined(SCHEDULER_DYNAMIC)
#include "./schedulers/dynamic/scheduler.h"
#elif defined(SCHEDULER_STATIC)
#include "schedulers/static/scheduler.h"
#else
#error "No scheduler specified"
#endif

#endif
