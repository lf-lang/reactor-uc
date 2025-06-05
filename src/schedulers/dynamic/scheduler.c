#include <string.h>

#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/port.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/tag.h"

/**
 * @brief Builtin triggers (startup/shutdown) are chained together as a linked
 * list and to prepare such a trigger we must iterate through the list.
 */
static void Scheduler_prepare_builtin(Event *event) {
  Trigger *trigger = event->trigger;
  do {
    trigger->prepare(trigger, event);
    trigger = (Trigger *)((BuiltinTrigger *)trigger)->next;
  } while (trigger);
}

static void Scheduler_pop_system_events_and_handle(Scheduler *untyped_self, tag_t next_tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;
  lf_ret_t ret;

  validate(self->system_event_queue);
  validate(self->system_event_queue->empty(self->system_event_queue) == false);

  do {
    ArbitraryEvent _event;
    SystemEvent *system_event = &_event.system_event;

    ret = self->system_event_queue->pop(self->system_event_queue, &system_event->super);
    validate(ret == LF_OK);
    validate(system_event->super.type == SYSTEM_EVENT);
    assert(lf_tag_compare(system_event->super.tag, next_tag) == 0);
    LF_DEBUG(SCHED, "Handling system event %p for tag " PRINTF_TAG, system_event, system_event->super.tag);

    system_event->handler->handle(system_event->handler, system_event);

  } while (lf_tag_compare(next_tag, self->system_event_queue->next_tag(self->system_event_queue)) == 0);
}

/**
 * @brief Pop off all the events from the event queue which have a tag matching
 * `next_tag` and prepare the associated triggers.
 */
static void Scheduler_pop_events_and_prepare(Scheduler *untyped_self, tag_t next_tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;
  lf_ret_t ret;

  if (lf_tag_compare(next_tag, self->event_queue->next_tag(self->event_queue)) != 0) {
    return;
  }

  do {
    ArbitraryEvent _event;
    Event *event = &_event.event;

    ret = self->event_queue->pop(self->event_queue, &event->super);

    // After popping the event, we dont need the critical section.

    validate(ret == LF_OK);

    validate(event->super.type == EVENT);
    assert(lf_tag_compare(event->super.tag, next_tag) == 0);
    LF_DEBUG(SCHED, "Handling event %p for tag " PRINTF_TAG, event, event->super.tag);

    Trigger *trigger = event->trigger;
    if (trigger->type == TRIG_STARTUP || trigger->type == TRIG_SHUTDOWN) {
      Scheduler_prepare_builtin(event);
    } else {
      trigger->prepare(trigger, event);
    }

  } while (lf_tag_compare(next_tag, self->event_queue->next_tag(self->event_queue)) == 0);
}

void Scheduler_register_for_cleanup(Scheduler *untyped_self, Trigger *trigger) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  LF_DEBUG(SCHED, "Registering trigger %p for cleanup", trigger);
  if (trigger->is_registered_for_cleanup) {
    return;
  }

  if (self->cleanup_ll_head == NULL) {
    assert(self->cleanup_ll_tail == NULL);
    self->cleanup_ll_head = trigger;
    self->cleanup_ll_tail = trigger;
  } else {
    self->cleanup_ll_tail->next = trigger;
    self->cleanup_ll_tail = trigger;
  }
  trigger->is_registered_for_cleanup = true;
}

void Scheduler_prepare_timestep(Scheduler *untyped_self, tag_t tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  // Before setting `current_tag` we must lock because it is read from async and channel context.
  MUTEX_LOCK(self->mutex);
  self->current_tag = tag;
  MUTEX_UNLOCK(self->mutex);

  self->reaction_queue->reset(self->reaction_queue);
}

void Scheduler_clean_up_timestep(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  assert(self->reaction_queue->empty(self->reaction_queue));

  assert(self->cleanup_ll_head && self->cleanup_ll_tail);
  LF_DEBUG(SCHED, "Cleaning up timestep for tag " PRINTF_TAG, self->current_tag);
  Trigger *cleanup_trigger = self->cleanup_ll_head;

  while (cleanup_trigger) {
    Trigger *this = cleanup_trigger;
    assert(!(this->next == NULL && this != self->cleanup_ll_tail));
    this->cleanup(this);
    this->is_registered_for_cleanup = false;
    cleanup_trigger = this->next;
    this->next = NULL;
  }

  self->cleanup_ll_head = NULL;
  self->cleanup_ll_tail = NULL;
}

/**
 * @brief Checks for safe-to-prcess violations for the given reaction. If a violation is detected
 * the violation handler is called.
 *
 * @param self
 * @param reaction
 * @return true if a violation was detected and handled, false otherwise.
 */
static bool _Scheduler_check_and_handle_stp_violations(DynamicScheduler *self, Reaction *reaction) {
  Reactor *parent = reaction->parent;
  for (size_t i = 0; i < parent->triggers_size; i++) {
    Trigger *trigger = parent->triggers[i];
    if (trigger->type == TRIG_INPUT && trigger->is_present) {
      Port *port = (Port *)trigger;
      if (lf_tag_compare(port->intended_tag, self->current_tag) == 0) {
        continue;
      }

      for (size_t j = 0; j < port->effects.size; j++) {
        if (port->effects.reactions[j] == reaction) {
          LF_WARN(SCHED, "Timeout detected for %s->reaction_%d", reaction->parent->name, reaction->index);
          reaction->stp_violation_handler(reaction);
          return true;
        }
      }
      for (size_t j = 0; j < port->observers.size; j++) {
        if (port->observers.reactions[j] == reaction) {
          LF_WARN(SCHED, "Timeout detected for %s->reaction_%d", reaction->parent->name, reaction->index);
          reaction->stp_violation_handler(reaction);
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * @brief Checks for deadline violations for the given reaction. If a violation is detected
 * the violation handler is called.
 *
 * @param self
 * @param reaction
 * @return true if a violation was detected and handled, false otherwise.
 */
static bool _Scheduler_check_and_handle_deadline_violations(DynamicScheduler *self, Reaction *reaction) {
  if (self->env->get_lag(self->env) >= reaction->deadline) {
    LF_WARN(SCHED, "Deadline violation detected for %s->reaction_%d", reaction->parent->name, reaction->index);
    reaction->deadline_violation_handler(reaction);
    return true;
  }
  return false;
}

void Scheduler_run_timestep(Scheduler *untyped_self) {
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

    LF_DEBUG(SCHED, "Executing %s->reaction_%d", reaction->parent->name, reaction->index);
    reaction->body(reaction);
  }
}

void Scheduler_do_shutdown(Scheduler *untyped_self, tag_t shutdown_tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  LF_INFO(SCHED, "Scheduler terminating at tag " PRINTF_TAG, shutdown_tag);
  self->super.prepare_timestep(untyped_self, shutdown_tag);

  Scheduler_pop_events_and_prepare(untyped_self, shutdown_tag);

  Trigger *shutdown = &self->env->shutdown->super;

  Event event = EVENT_INIT(shutdown_tag, shutdown, NULL);
  if (shutdown) {
    Scheduler_prepare_builtin(&event);

    self->run_timestep(untyped_self);
    self->clean_up_timestep(untyped_self);
  }
}

void Scheduler_set_and_schedule_start_tag(Scheduler *untyped_self, instant_t start_time) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  // Set start and stop tags. This is always called from the runtime context. But asynchronous and channel context
  // read start_time and stop_tag when calling `Scheduler_schedule_at` and thus we must lock before updating them.
  MUTEX_LOCK(self->mutex);
  tag_t stop_tag = {.time = lf_time_add(start_time, untyped_self->duration), .microstep = 0};
  untyped_self->start_time = start_time;
  self->stop_tag = stop_tag;
  self->super.running = true;
  MUTEX_UNLOCK(self->mutex);

  // Initial events will be scheduled by the Startup Coordinator, based on which policy was selected
}

void Scheduler_run(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  Environment *env = self->env;
  lf_ret_t res;
  tag_t start_tag = {.time = untyped_self->start_time, .microstep = 0};
  tag_t next_tag = NEVER_TAG;
  tag_t next_system_tag = FOREVER_TAG;
  bool going_to_shutdown = false;
  bool next_event_is_system_event = false;
  LF_DEBUG(SCHED, "Scheduler running with keep_alive=%d", self->super.keep_alive);

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
    res = self->env->wait_until(self->env, next_tag.time);

    if (res == LF_SLEEP_INTERRUPTED) {
      LF_DEBUG(SCHED, "Sleep interrupted before completion");
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

lf_ret_t Scheduler_schedule_at(Scheduler *super, Event *event) {
  DynamicScheduler *self = (DynamicScheduler *)super;
  lf_ret_t ret;

  // This can be called from the async context and the channel context. It reads stop_tag, current_tag, start_time
  // and more and we lock the scheduler mutex before doing anything.
  MUTEX_LOCK(self->mutex);

  // Check if we are trying to schedule past stop tag
  if (lf_tag_compare(event->super.tag, self->stop_tag) > 0) {
    LF_WARN(SCHED, "Trying to schedule event at tag " PRINTF_TAG " past stop tag " PRINTF_TAG, event->super.tag,
            self->stop_tag);
    ret = LF_AFTER_STOP_TAG;
    goto unlock_and_return;
  }

  // Check if we are trying to schedule into the past
  if (lf_tag_compare(event->super.tag, self->current_tag) <= 0) {
    LF_WARN(SCHED, "Trying to schedule event at tag " PRINTF_TAG " which is before current tag " PRINTF_TAG,
            event->super.tag, self->current_tag);
    ret = LF_PAST_TAG;
    goto unlock_and_return;
  }

  // Check if we are trying to schedule before the start tag
  if (self->super.start_time > 0) {
    tag_t start_tag = {.time = self->super.start_time, .microstep = 0};
    if (lf_tag_compare(event->super.tag, start_tag) < 0 || self->super.start_time == NEVER) {
      LF_WARN(SCHED, "Trying to schedule event at tag " PRINTF_TAG " which is before start tag", event->super.tag);
      ret = LF_INVALID_TAG;
      goto unlock_and_return;
    }
  }

  ret = self->event_queue->insert(self->event_queue, (AbstractEvent *)event);
  validate(ret == LF_OK);

  self->env->platform->notify(self->env->platform);

unlock_and_return:
  MUTEX_UNLOCK(self->mutex);
  return ret;
}

lf_ret_t Scheduler_schedule_system_event_at(Scheduler *super, SystemEvent *event) {
  DynamicScheduler *self = (DynamicScheduler *)super;
  lf_ret_t ret;
  MUTEX_LOCK(self->mutex);

  ret = self->system_event_queue->insert(self->system_event_queue, (AbstractEvent *)event);
  validate(ret == LF_OK);
  MUTEX_UNLOCK(self->mutex);

  self->env->platform->notify(self->env->platform);
  return LF_OK;
}

void Scheduler_set_duration(Scheduler *self, interval_t duration) {
  self->duration = duration;
}

void Scheduler_request_shutdown(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  Environment *env = self->env;
  // request shutdown is called from reactions which are not executed in critical sections.
  // Thus we enter a critical section before setting the stop tag.
  MUTEX_LOCK(self->mutex);
  self->stop_tag = lf_delay_tag(self->current_tag, 0);
  LF_INFO(SCHED, "Shutdown requested, will stop at tag" PRINTF_TAG, self->stop_tag);
  env->platform->notify(env->platform);
  MUTEX_UNLOCK(self->mutex);
}

/** If the clock is stepped, forward or backward, we need to adjust the tags of all events in the system event queue. */
static void Scheduler_step_clock(Scheduler *_self, interval_t step) {
  DynamicScheduler *self = (DynamicScheduler *)_self;
  EventQueue *q = self->system_event_queue;

  // Note that we must lock the mutex of the queue, not the scheduler to do this!
  MUTEX_LOCK(q->mutex);
  for (size_t i = 0; i < q->size; i++) {
    ArbitraryEvent event = q->array[i];
    instant_t old_tag = event.system_event.super.tag.time;
    instant_t new_tag = old_tag + step;
    if (new_tag < 0) {
      new_tag = 0;
    }
    event.system_event.super.tag.time = new_tag;
  }
  MUTEX_UNLOCK(q->mutex);
}

lf_ret_t Scheduler_add_to_reaction_queue(Scheduler *untyped_self, Reaction *reaction) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  return self->reaction_queue->insert(self->reaction_queue, reaction);
}

tag_t Scheduler_current_tag(Scheduler *untyped_self) {
  return ((DynamicScheduler *)untyped_self)->current_tag;
}

void DynamicScheduler_ctor(DynamicScheduler *self, Environment *env, EventQueue *event_queue,
                           EventQueue *system_event_queue, ReactionQueue *reaction_queue, interval_t duration,
                           bool keep_alive) {
  self->env = env;

  self->super.keep_alive = keep_alive;
  self->super.duration = duration;
  self->stop_tag = FOREVER_TAG;
  self->current_tag = NEVER_TAG;
  self->cleanup_ll_head = NULL;
  self->cleanup_ll_tail = NULL;
  self->event_queue = event_queue;
  self->reaction_queue = reaction_queue;
  self->system_event_queue = system_event_queue;

  self->super.start_time = NEVER;
  self->super.running = false;
  self->super.run = Scheduler_run;
  self->clean_up_timestep = Scheduler_clean_up_timestep;
  self->run_timestep = Scheduler_run_timestep;

  self->super.prepare_timestep = Scheduler_prepare_timestep;
  self->super.do_shutdown = Scheduler_do_shutdown;
  self->super.schedule_at = Scheduler_schedule_at;
  self->super.schedule_system_event_at = Scheduler_schedule_system_event_at;
  self->super.register_for_cleanup = Scheduler_register_for_cleanup;
  self->super.request_shutdown = Scheduler_request_shutdown;
  self->super.set_and_schedule_start_tag = Scheduler_set_and_schedule_start_tag;
  self->super.add_to_reaction_queue = Scheduler_add_to_reaction_queue;
  self->super.current_tag = Scheduler_current_tag;
  self->super.step_clock = Scheduler_step_clock;

  Mutex_ctor(&self->mutex.super);
}
