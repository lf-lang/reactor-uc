#ifndef REACTOR_UC_SCHEDULER_H
#define REACTOR_UC_SCHEDULER_H

#include "reactor-uc/queues.h"
#include "reactor-uc/event.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"

typedef struct Scheduler Scheduler;
typedef struct Environment Environment;
typedef struct LevelDeferredAction LevelDeferredAction;

/**
 * @brief An action that is executed once all reactions up to (and including) a given level have been
 * executed at the current tag, i.e. right before the first reaction with a higher level is executed, or at
 * the end of the tag if there is none.
 *
 * Used by federated output connections to send their value once all reactions that can write it have
 * executed ("last write wins"), without a reaction per output connection. The struct is meant to be embedded
 * into its owner, which can be recovered from `execute` via `CONTAINER_OF`.
 */
struct LevelDeferredAction {
  LevelDeferredAction* next;                  // Intrusive list of the pending actions, owned by the scheduler.
  int level;                                  // Executed after all reactions with level <= `level`.
  bool pending;                               // True while registered for the current tag.
  void (*execute)(LevelDeferredAction* self); // Called by the scheduler.
};

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

  void (*request_shutdown)(Scheduler* self, tag_t shutdown_time, bool overwrite);

  /**
   * @brief Register Trigger for cleanup. The cleanup function of the trigger
   * will be called in `clean_up_timestep`.
   */
  void (*register_for_cleanup)(Scheduler* self, Trigger* trigger);

  void (*set_and_schedule_start_tag)(Scheduler* self, instant_t start_time);

  // void (*set_duration)(Scheduler *self, interval_t duration);

  lf_ret_t (*add_to_reaction_queue)(Scheduler* self, Reaction* reaction);

  /**
   * @brief Execute `action` at the current tag once all reactions with a level <= `action->level` have been
   * executed. Registering an action that is already pending has no effect, so it is executed at most once
   * per tag. Pending actions are executed in order of their level, and in registration order within a level.
   * Must be called from the scheduler thread (i.e. from a reaction).
   */
  void (*defer_until_level_done)(Scheduler* self, LevelDeferredAction* action);

  tag_t (*current_tag)(Scheduler* self);

  /**
   * @brief After committing to a tag, but before executing reactions, the
   * scheduler must prepare the timestep by adding reactions to the reaction
   * queue.
   */
  void (*prepare_timestep)(Scheduler* self, tag_t tag);

  /**
   * @brief Cancel a pending event for a `trigger` at `event_time`. Returns
   * LF_EVENT_NOT_FOUND if no matching event exists.
   */
  lf_ret_t (*cancel_event)(Scheduler* self, Trigger* trigger, instant_t event_time);

  /**
   * @brief Replace the payload of a pending event for a `trigger` at
   * `event_time`. Returns LF_EVENT_NOT_FOUND if no matching event exists.
   */
  lf_ret_t (*replace_event_payload)(Scheduler* self, Trigger* trigger, instant_t event_time, const void* new_value);
};

Scheduler* Scheduler_new(Environment* env, EventQueue* event_queue, EventQueue* system_event_queue,
                         ReactionQueue* reaction_queue, interval_t duration, bool keep_alive);

#endif
