
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/tag.h"

static DynamicScheduler scheduler;

// Private functions

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

  do {
    ArbitraryEvent _event;
    Event *event = &_event.event;

    ret = self->event_queue->pop(self->event_queue, &event->super);
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

/**
 * @brief Acquire a tag by iterating through all network input ports and making
 * sure that they are resolved at this tag. If the input port is unresolved we
 * must wait for the max_wait time before proceeding.
 *
 * @param self
 * @param next_tag
 * @return lf_ret_t
 */
static lf_ret_t Scheduler_federated_acquire_tag(Scheduler *untyped_self, tag_t next_tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  LF_DEBUG(SCHED, "Acquiring tag " PRINTF_TAG, next_tag);
  Environment *env = self->env;
  instant_t additional_sleep = 0;
  for (size_t i = 0; i < env->net_bundles_size; i++) {
    FederatedConnectionBundle *bundle = env->net_bundles[i];

    if (!bundle->net_channel->is_connected(bundle->net_channel)) {
      continue;
    }

    for (size_t j = 0; j < bundle->inputs_size; j++) {
      FederatedInputConnection *input = bundle->inputs[j];
      // Find the max safe-to-assume-absent value and go to sleep waiting for this.
      if (lf_tag_compare(input->last_known_tag, next_tag) < 0) {
        LF_DEBUG(SCHED, "Input %p is unresolved, latest known tag was " PRINTF_TAG, input, input->last_known_tag);
        LF_DEBUG(SCHED, "Input %p has maxwait of  " PRINTF_TIME, input, input->max_wait);
        if (input->max_wait > additional_sleep) {
          additional_sleep = input->max_wait;
        }
      }
    }
  }

  if (additional_sleep > 0) {
    LF_DEBUG(SCHED, "Need to sleep for additional " PRINTF_TIME " ns", additional_sleep);
    instant_t sleep_until = lf_time_add(next_tag.time, additional_sleep);
    return env->wait_until(env, sleep_until);
  } else {
    return LF_OK;
  }
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

  LF_DEBUG(SCHED, "Preparing timestep for tag " PRINTF_TAG, tag);
  self->current_tag = tag;
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
  if (self->env->get_physical_time(self->env) > (self->current_tag.time + reaction->deadline)) {
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
  Environment *env = self->env;
  self->prepare_timestep(untyped_self, shutdown_tag);
  env->leave_critical_section(env);

  Trigger *shutdown = &self->env->shutdown->super;

  Event event = EVENT_INIT(shutdown_tag, shutdown, NULL);
  if (shutdown) {
    Scheduler_prepare_builtin(&event);
    self->run_timestep(untyped_self);
    self->clean_up_timestep(untyped_self);
  }
}

void Scheduler_schedule_startups(Scheduler *self, tag_t start_tag) {
  Environment *env = ((DynamicScheduler *)self)->env;
  if (env->startup) {
    Event event = EVENT_INIT(start_tag, &env->startup->super, NULL);
    lf_ret_t ret = self->schedule_at_locked(self, &event.super);
    validate(ret == LF_OK);
  }
}

void Scheduler_schedule_timers(Scheduler *self, Reactor *reactor, tag_t start_tag) {
  lf_ret_t ret;
  for (size_t i = 0; i < reactor->triggers_size; i++) {
    Trigger *trigger = reactor->triggers[i];
    if (trigger->type == TRIG_TIMER) {
      Timer *timer = (Timer *)trigger;
      tag_t tag = {.time = start_tag.time + timer->offset, .microstep = start_tag.microstep};
      Event event = EVENT_INIT(tag, &timer->super, NULL);
      ret = self->schedule_at_locked(self, &event.super);
      validate(ret == LF_OK);
    }
  }
  for (size_t i = 0; i < reactor->children_size; i++) {
    Scheduler_schedule_timers(self, reactor->children[i], start_tag);
  }
}

void Scheduler_set_and_schedule_start_tag(Scheduler *untyped_self, instant_t start_time) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;
  Environment *env = self->env;
  env->enter_critical_section(env);

  // Set start and stop tags
  tag_t start_tag = {.time = start_time, .microstep = 0};
  untyped_self->start_time = start_time;
  self->stop_tag = lf_delay_tag(start_tag, untyped_self->duration);

  // Schedule the initial events
  Scheduler_schedule_startups(untyped_self, start_tag);
  Scheduler_schedule_timers(untyped_self, env->main, start_tag);
  env->leave_critical_section(env);
}

void Scheduler_run(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  Environment *env = self->env;
  lf_ret_t res;
  tag_t next_tag = NEVER_TAG;
  tag_t next_system_tag = FOREVER_TAG;
  bool non_terminating = self->super.keep_alive || env->has_async_events;
  bool going_to_shutdown = false;
  bool next_event_is_system_event = false;
  LF_DEBUG(SCHED, "Scheduler running with non_terminating=%d has_async_events=%d", non_terminating,
           env->has_async_events);

  env->enter_critical_section(env);

  while (non_terminating || !self->event_queue->empty(self->event_queue)) {
    next_tag = self->event_queue->next_tag(self->event_queue);
    if (self->system_event_queue) {
      next_system_tag = self->system_event_queue->next_tag(self->system_event_queue);
    }

    if (lf_tag_compare(next_tag, next_system_tag) > 0) {
      next_tag = next_system_tag;
      next_event_is_system_event = true;
      LF_DEBUG(SCHED, "Next event is a system_event at " PRINTF_TAG, next_tag);
    } else {
      next_event_is_system_event = false;
      LF_DEBUG(SCHED, "Next event is at " PRINTF_TAG, next_tag);
    }

    if (lf_tag_compare(next_tag, self->stop_tag) > 0) {
      LF_DEBUG(SCHED, "Next event is beyond stop tag: " PRINTF_TAG, self->stop_tag);
      next_tag = self->stop_tag;
      going_to_shutdown = true;
      next_event_is_system_event = false;
    }

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
    if (self->env->is_federated) {
      res = Scheduler_federated_acquire_tag(untyped_self, next_tag);
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

    self->prepare_timestep(untyped_self, next_tag);

    Scheduler_pop_events_and_prepare(untyped_self, next_tag);
    LF_DEBUG(SCHED, "Acquired tag %" PRINTF_TAG, next_tag);

    env->leave_critical_section(env);

    self->run_timestep(untyped_self);
    self->clean_up_timestep(untyped_self);
    env->enter_critical_section(env);
  }

  // Figure out which tag which should execute shutdown at.
  tag_t shutdown_tag;
  if (!non_terminating && self->event_queue->empty(self->event_queue)) {
    LF_DEBUG(SCHED, "Shutting down due to starvation.");
    shutdown_tag = lf_delay_tag(self->current_tag, 0);
  } else {
    LF_DEBUG(SCHED, "Shutting down because we reached the stop tag.");
    shutdown_tag = self->stop_tag;
  }

  self->super.do_shutdown(untyped_self, shutdown_tag);
}

lf_ret_t Scheduler_schedule_at_locked(Scheduler *untyped_self, AbstractEvent *event) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;
  lf_ret_t ret;

  if (event->type == EVENT) {
    // Check if we are trying to schedule past stop tag
    if (lf_tag_compare(event->tag, self->stop_tag) > 0) {
      LF_WARN(SCHED, "Trying to schedule event at tag " PRINTF_TAG " past stop tag " PRINTF_TAG, event->tag,
              self->stop_tag);
      return LF_AFTER_STOP_TAG;
    }

    // Check if we are tring to schedule into the past
    if (lf_tag_compare(event->tag, self->current_tag) <= 0) {
      LF_WARN(SCHED, "Trying to schedule event at tag " PRINTF_TAG " which is before current tag " PRINTF_TAG,
              event->tag, self->current_tag);
      return LF_PAST_TAG;
    }

    // Check if we are trying to schedule before the start tag
    tag_t start_tag = {.time = self->super.start_time, .microstep = 0};
    if (lf_tag_compare(event->tag, start_tag) < 0 || self->super.start_time == NEVER) {
      LF_WARN(SCHED, "Trying to schedule event at tag " PRINTF_TAG " which is before start tag", event->tag);
      return LF_INVALID_TAG;
    }

    ret = self->event_queue->insert(self->event_queue, event);
    if (ret != LF_OK) {
      LF_ERR(SCHED, "Failed to insert event into event queue");
    }
  } else {
    ret = self->system_event_queue->insert(self->system_event_queue, event);
    if (ret != LF_OK) {
      LF_ERR(SCHED, "Failed to insert system event into event queue");
    }
  }

  if (ret == LF_OK) {
    self->env->platform->new_async_event(self->env->platform);
  }

  return ret;
}

lf_ret_t Scheduler_schedule_at(Scheduler *self, AbstractEvent *event) {
  Environment *env = ((DynamicScheduler *)self)->env;

  env->enter_critical_section(env);

  int res = self->schedule_at_locked(self, event);

  env->leave_critical_section(env);

  return res;
}

void Scheduler_set_duration(Scheduler *self, interval_t duration) { self->duration = duration; }

void Scheduler_request_shutdown(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  Environment *env = self->env;
  env->enter_critical_section(env);
  self->stop_tag = lf_delay_tag(self->current_tag, 0);
  LF_INFO(SCHED, "Shutdown requested, will stop at tag" PRINTF_TAG, self->stop_tag.time);
  env->platform->new_async_event(env->platform);
  env->leave_critical_section(env);
}

lf_ret_t Scheduler_add_to_reaction_queue(Scheduler *untyped_self, Reaction *reaction) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  return self->reaction_queue->insert(self->reaction_queue, reaction);
}

tag_t Scheduler_current_tag(Scheduler *untyped_self) { return ((DynamicScheduler *)untyped_self)->current_tag; }

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
  self->super.run = Scheduler_run;
  self->prepare_timestep = Scheduler_prepare_timestep;
  self->clean_up_timestep = Scheduler_clean_up_timestep;
  self->run_timestep = Scheduler_run_timestep;
  self->super.do_shutdown = Scheduler_do_shutdown;
  self->super.schedule_at = Scheduler_schedule_at;
  self->super.schedule_at_locked = Scheduler_schedule_at_locked;
  self->super.register_for_cleanup = Scheduler_register_for_cleanup;
  self->super.request_shutdown = Scheduler_request_shutdown;
  self->super.set_and_schedule_start_tag = Scheduler_set_and_schedule_start_tag;
  self->super.add_to_reaction_queue = Scheduler_add_to_reaction_queue;
  self->super.current_tag = Scheduler_current_tag;
}

Scheduler *Scheduler_new(Environment *env, EventQueue *event_queue, EventQueue *system_event_queue,
                         ReactionQueue *reaction_queue, interval_t duration, bool keep_alive) {
  DynamicScheduler_ctor(&scheduler, env, event_queue, system_event_queue, reaction_queue, duration, keep_alive);
  return (Scheduler *)&scheduler;
}
