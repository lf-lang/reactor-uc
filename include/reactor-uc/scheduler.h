#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H

#include "reactor-uc/queues.h"
#include "reactor-uc/event.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"

typedef struct Scheduler Scheduler;
typedef struct Environment Environment;

struct Scheduler {
  bool running;
  interval_t start_time;
  interval_t duration; // The duration after which the program should stop.
  bool keep_alive;     // Whether the program should keep running even if there are no more events to process.

  /**
   * @brief Schedules an event on trigger at a specified tag. This function will
   * enter a critcal section if the environment has async events.
   */
  lf_ret_t (*schedule_at)(Scheduler* self, Event* event);

  lf_ret_t (*schedule_system_event_at)(Scheduler* self, SystemEvent* event);

  /**
   * @brief Runs the program. Does not return until program has completed.
   */
  void (*run)(Scheduler* self);

  void (*step_clock)(Scheduler* self, interval_t step);

  /**
   * @brief Called to execute all reactions triggered by a shutdown trigger.
   */
  void (*do_shutdown)(Scheduler* self, tag_t stop_tag);

  void (*request_shutdown)(Scheduler* self);

  /**
   * @brief Register Trigger for cleanup. The cleanup function of the trigger
   * will be called in `clean_up_timestep`.
   */
  void (*register_for_cleanup)(Scheduler* self, Trigger* trigger);

  void (*set_and_schedule_start_tag)(Scheduler* self, instant_t start_time);

  // void (*set_duration)(Scheduler *self, interval_t duration);

  lf_ret_t (*add_to_reaction_queue)(Scheduler* self, Reaction* reaction);

  tag_t (*current_tag)(Scheduler* self);

  /**
   * @brief After committing to a tag, but before executing reactions, the
   * scheduler must prepare the timestep by adding reactions to the reaction
   * queue.
   */
  void (*prepare_timestep)(Scheduler* self, tag_t tag);
};

Scheduler* Scheduler_new(Environment* env, EventQueue* event_queue, EventQueue* system_event_queue,
                         ReactionQueue* reaction_queue, interval_t duration, bool keep_alive);

#endif
