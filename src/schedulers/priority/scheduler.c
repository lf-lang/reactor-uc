#include <string.h>

#include "reactor-uc/schedulers/priority/scheduler.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/port.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/tag.h"

void PriorityScheduler_run_timestep(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  while (!self->reaction_queue->empty(self->reaction_queue)) {
    Reaction *reaction = self->reaction_queue->pop(self->reaction_queue);

    if (reaction->stp_violation_handler != NULL) {
      if (_Scheduler_check_and_handle_stp_violations(self, reaction)) {
        continue;
      }
    }

    if (reaction->deadline_violation_handler != NULL) {
      if (_Scheduler_check_and_handle_deadline_violations(self, reaction)) {
        continue;
      }
    }

    // Setting the priority of the current thread before executing the reaction
    // (the function throws an exception if an error occurs)
    
    // Cast is safe because the PriorityScheduler is only supported by ThreadedPlatforms
    ThreadedPlatform* platform = (ThreadedPlatform*)(self->env->platform);
    platform->set_thread_priority(reaction->deadline);

    LF_DEBUG(SCHED, "Executing %s->reaction_%d", reaction->parent->name, reaction->index);
    reaction->body(reaction);
  }
}

void PriorityScheduler_run(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  Environment *env = self->env;
  lf_ret_t res;
  tag_t start_tag = {.time = untyped_self->start_time, .microstep = 0};
  tag_t next_tag = NEVER_TAG;
  tag_t next_system_tag = FOREVER_TAG;
  bool going_to_shutdown = false;
  bool next_event_is_system_event = false;
  LF_DEBUG(SCHED, "Scheduler running with keep_alive=%d", self->super.keep_alive);

  // Setting the appropriate scheduling class before starting handling events
  // (the function throws an exception if an error occurs)
  
  // Cast is safe because the PriorityScheduler is only supported by ThreadedPlatforms
  ThreadedPlatform* platform = (ThreadedPlatform*)(self->env->platform);
  platform->set_scheduling_policy();

  // Setting the appropriate core affinity before starting handling events
  // (the function throws an exception if an error occurs)
  platform->set_core_affinity();

  while (self->super.keep_alive || !self->event_queue->empty(self->event_queue) || untyped_self->start_time == NEVER) {
    next_tag = self->event_queue->next_tag(self->event_queue);

    // Check that next tag is greater than start tag. Could be violated if we are scheduling events when the start
    // tag is decided, or receive an input in that period.
    if (lf_tag_compare(next_tag, start_tag) < 0) {
      LF_WARN(SCHED, "Dropping event with tag " PRINTF_TAG " because it is before start tag " PRINTF_TAG, next_tag,
              start_tag);
      ArbitraryEvent e;
      self->event_queue->pop(self->event_queue, (AbstractEvent *)&e);
      e.event.trigger->payload_pool->free(e.event.trigger->payload_pool, e.event.super.payload);
      continue;
    }

    if (env->poll_network_channels) {
      env->poll_network_channels(env);
    }

    // If we have system events, we need to check if the next event is a system event.
    if (self->system_event_queue) {
      next_system_tag = self->system_event_queue->next_tag(self->system_event_queue);
    }

    // Handle the one with lower tag, if they are equal, prioritize normal events.
    if (lf_tag_compare(next_tag, next_system_tag) > 0) {
      next_tag = next_system_tag;
      next_event_is_system_event = true;
      LF_DEBUG(SCHED, "Next event is a system_event at " PRINTF_TAG, next_tag);
    } else {
      next_event_is_system_event = false;
      LF_DEBUG(SCHED, "Next event is at " PRINTF_TAG, next_tag);
    }

    // Detect if event is past the stop tag, in which case we go to shutdown instead.
    if (lf_tag_compare(next_tag, self->stop_tag) >= 0) {
      LF_DEBUG(SCHED, "Next event is beyond or at stop tag: " PRINTF_TAG, self->stop_tag);
      next_tag = self->stop_tag;
      going_to_shutdown = true;
      next_event_is_system_event = false;
    }

    // We have found the next tag we want to handle. Wait until physical time reaches this tag.
    
    // Setting the second maximum priority for the current thread before sleeping, otherwise priority
    // inversion may happen that delays the wake-up
    // (the function throws an exception if an error occurs)
    platform->set_thread_priority(LF_SLEEP_PRIORITY);

    res = self->env->wait_until(self->env, next_tag.time);

    if (res == LF_SLEEP_INTERRUPTED) {
      LF_DEBUG(SCHED, "Sleep interrupted before completion");
      // Reset the shutdown flag in the case it was set before sleeping:
      // now there is another event to handle before the stop tag
      going_to_shutdown = false;
      continue;
    } else if (res != LF_OK) {
      throw("Sleep failed");
    }

    if (next_event_is_system_event) {
      Scheduler_pop_system_events_and_handle(untyped_self, next_tag);
      continue;
    }

    // For federated execution, acquire next_tag before proceeding. This function
    // might sleep and will return LF_SLEEP_INTERRUPTED if sleep was interrupted.
    // If this is the shutdown tag, we do not acquire the tag to ensure that we always terminate.
    if (self->env->acquire_tag && !going_to_shutdown) {
      res = self->env->acquire_tag(self->env, next_tag);
      if (res == LF_SLEEP_INTERRUPTED) {
        LF_DEBUG(SCHED, "Sleep interrupted while waiting for federated input to resolve.");
        continue;
      }
    }
    // Once we are here, we have are committed to executing `next_tag`.

    // If we have reached the stop tag, we break the while loop and go to termination.
    if (lf_tag_compare(next_tag, self->stop_tag) == 0 && going_to_shutdown) {
      break;
    }

    self->super.prepare_timestep(untyped_self, next_tag);

    Scheduler_pop_events_and_prepare(untyped_self, next_tag);
    LF_DEBUG(SCHED, "Acquired tag %" PRINTF_TAG, next_tag);

    // Emptying the reaction queue, executing all reactions and cleaning up the tag
    // can be done outside the critical section.
    self->run_timestep(untyped_self);
    self->clean_up_timestep(untyped_self);
  }

  // Figure out which tag which should execute shutdown at.
  tag_t shutdown_tag;
  if (!self->super.keep_alive && self->event_queue->empty(self->event_queue)) {
    LF_DEBUG(SCHED, "Shutting down due to starvation.");
    shutdown_tag = lf_delay_tag(self->current_tag, 0);
  } else {
    LF_DEBUG(SCHED, "Shutting down because we reached the stop tag.");
    shutdown_tag = self->stop_tag;
  }

  self->super.do_shutdown(untyped_self, shutdown_tag);
}

void PriorityScheduler_ctor(PriorityScheduler *self, Environment *env, EventQueue *event_queue,
                           EventQueue *system_event_queue, ReactionQueue *reaction_queue, interval_t duration,
                           bool keep_alive) {

  DynamicScheduler* super = (DynamicScheduler*)self;
  DynamicScheduler_ctor(super, env, event_queue, system_event_queue, reaction_queue, duration, keep_alive);

  super->run_timestep = PriorityScheduler_run_timestep;
  super->super.run = PriorityScheduler_run;

  Mutex_ctor(&super->mutex.super);
}
