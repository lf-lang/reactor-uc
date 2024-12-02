
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/timer.h"

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

/**
 * @brief Pop off all the events from the event queue which have a tag matching
 * `next_tag` and prepare the associated triggers.
 */
static void Scheduler_pop_events_and_prepare(Scheduler *untyped_self, tag_t next_tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  do {
    Event event = self->event_queue.pop(&self->event_queue);
    assert(lf_tag_compare(event.tag, next_tag) == 0);
    LF_DEBUG(SCHED, "Handling event %p for tag %" PRId64 ":%" PRIu32, &event, event.tag.time, event.tag.microstep);

    Trigger *trigger = event.trigger;
    if (trigger->type == TRIG_STARTUP || trigger->type == TRIG_SHUTDOWN) {
      Scheduler_prepare_builtin(&event);
    } else {
      trigger->prepare(trigger, &event);
    }
  } while (lf_tag_compare(next_tag, self->event_queue.next_tag(&self->event_queue)) == 0);
}

/**
 * @brief Acquire a tag by iterating through all network input ports and making
 * sure that they are resolved at this tag. If the input port is unresolved we
 * must wait for the safe_to_assume_absent time before proceeding.
 *
 * @param self
 * @param next_tag
 * @return lf_ret_t
 */
static lf_ret_t Scheduler_federated_acquire_tag(Scheduler *untyped_self, tag_t next_tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  LF_DEBUG(SCHED, "Acquiring tag %" PRId64 ":%" PRIu32, next_tag.time, next_tag.microstep);
  Environment *env = self->env;
  instant_t additional_sleep = 0;
  for (size_t i = 0; i < env->net_bundles_size; i++) {
    FederatedConnectionBundle *bundle = env->net_bundles[i];
    for (size_t j = 0; j < bundle->inputs_size; j++) {
      FederatedInputConnection *input = bundle->inputs[j];
      validate(input->safe_to_assume_absent == FOREVER); // TODO: We only support dataflow like things now
      // Find the max safe-to-assume-absent value and go to sleep waiting for this.
      if (lf_tag_compare(input->last_known_tag, next_tag) < 0) {
        LF_DEBUG(SCHED, "Input %p is unresolved, latest known tag was %" PRId64 ":%" PRIu32, input,
                 input->last_known_tag.time, input->last_known_tag.microstep);
        LF_DEBUG(SCHED, "Input %p has STAA of  %" PRId64, input->safe_to_assume_absent);
        if (input->safe_to_assume_absent > additional_sleep) {
          additional_sleep = input->safe_to_assume_absent;
        }
      }
    }
  }

  if (additional_sleep > 0) {
    LF_DEBUG(SCHED, "Need to sleep for additional %" PRId64 " ns", additional_sleep);
    instant_t sleep_until = lf_time_add(env->get_logical_time(env), additional_sleep);
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

  LF_DEBUG(SCHED, "Preparing timestep for tag %" PRId64 ":%" PRIu32, tag.time, tag.microstep);
  self->current_tag = tag;
  self->reaction_queue.reset(&self->reaction_queue);
}

void Scheduler_clean_up_timestep(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  assert(self->reaction_queue.empty(&self->reaction_queue));

  assert(self->cleanup_ll_head && self->cleanup_ll_tail);
  LF_DEBUG(SCHED, "Cleaning up timestep for tag %" PRId64 ":%" PRIu32, self->current_tag.time,
           self->current_tag.microstep);
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

void Scheduler_run_timestep(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  while (!self->reaction_queue.empty(&self->reaction_queue)) {
    Reaction *reaction = self->reaction_queue.pop(&self->reaction_queue);
    assert(reaction);

    if (reaction->deadline_handler == NULL) {
      LF_DEBUG(SCHED, "Executing %s->reaction_%d", reaction->parent->name, reaction->index);
      reaction->body(reaction);
    } else if (self->env->get_physical_time(self->env) > (self->current_tag.time + reaction->deadline)) {
      LF_WARN(SCHED, "Deadline violation detected for %s->reaction_%d", reaction->parent->name, reaction->index);
      reaction->deadline_handler(reaction);
    } else {
      LF_DEBUG(SCHED, "Executing %s->reaction_%d", reaction->parent->name, reaction->index);
      reaction->body(reaction);
    }
  }
}

void Scheduler_do_shutdown(Scheduler *untyped_self, tag_t shutdown_tag) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  LF_INFO(SCHED, "Scheduler terminating at tag %" PRId64 ":%" PRIu32, shutdown_tag.time, shutdown_tag.microstep);
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
    self->schedule_at_locked(self, &event);
  }
}

void Scheduler_schedule_timers(Scheduler *self, Reactor *reactor, tag_t start_tag) {
  for (size_t i = 0; i < reactor->triggers_size; i++) {
    Trigger *trigger = reactor->triggers[i];
    if (trigger->type == TRIG_TIMER) {
      Timer *timer = (Timer *)trigger;
      tag_t tag = {.time = start_tag.time + timer->offset, .microstep = start_tag.microstep};
      Event event = EVENT_INIT(tag, &timer->super, NULL);
      self->schedule_at_locked(self, &event);
    }
  }
  for (size_t i = 0; i < reactor->children_size; i++) {
    Scheduler_schedule_timers(self, reactor->children[i], start_tag);
  }
}

void Scheduler_acquire_and_schedule_start_tag(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  Environment *env = self->env;
  env->enter_critical_section(env);
  if (env->net_bundles_size == 0) {
    self->super.start_time = env->get_physical_time(env);
    LF_DEBUG(SCHED, "No federated connections, picking start_time %" PRId64, self->super.start_time);
  } else if (self->super.leader) {
    self->super.start_time = env->get_physical_time(env);
    LF_DEBUG(SCHED, "Is leader of the federation, picking start_time %" PRId64, self->super.start_time);
    Federated_distribute_start_tag(env, self->super.start_time);
  } else {
    LF_DEBUG(SCHED, "Not leader, waiting for start tag signal");
    while (self->super.start_time == NEVER) {
      env->wait_until(env, FOREVER);
    }
  }
  LF_DEBUG(SCHED, "Start_time is %" PRId64, self->super.start_time);
  tag_t start_tag = {.time = self->super.start_time, .microstep = 0};
  // Set the stop tag
  self->stop_tag = lf_delay_tag(start_tag, self->super.duration);
  LF_DEBUG(INFO, "Start time is %" PRId64 "and stop time is %" PRId64 " (%" PRId32 ")", self->super.start_time,
           self->stop_tag.time, self->super.duration);

  // Schedule the initial events
  Scheduler_schedule_startups(untyped_self, start_tag);
  Scheduler_schedule_timers(untyped_self, env->main, start_tag);
  env->leave_critical_section(env);
}

void Scheduler_run(Scheduler *untyped_self) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  Environment *env = self->env;
  lf_ret_t res;
  tag_t next_tag;
  bool non_terminating = self->super.keep_alive || env->has_async_events;
  bool going_to_shutdown = false;
  LF_INFO(SCHED, "Scheduler running with non_terminating=%d has_async_events=%d", non_terminating,
          env->has_async_events);

  env->enter_critical_section(env);

  while (non_terminating || !self->event_queue.empty(&self->event_queue)) {
    next_tag = self->event_queue.next_tag(&self->event_queue);
    LF_DEBUG(SCHED, "Next event is at %" PRId64 ":%" PRIu32, next_tag.time, next_tag.microstep);

    if (lf_tag_compare(next_tag, self->stop_tag) > 0) {
      LF_INFO(SCHED, "Next event is beyond stop tag: %" PRId64 ":%" PRIu32, self->stop_tag.time,
              self->stop_tag.microstep);
      next_tag = self->stop_tag;
      going_to_shutdown = true;
    }

    res = self->env->wait_until(self->env, next_tag.time);
    if (res == LF_SLEEP_INTERRUPTED) {
      LF_DEBUG(SCHED, "Sleep interrupted before completion");
      continue;
    } else if (res != LF_OK) {
      throw("Sleep failed");
    }

    // For federated execution, acquire next_tag before proceeding. This function
    // might sleep and will return LF_SLEEP_INTERRUPTED if sleep was interrupted.
    res = Scheduler_federated_acquire_tag(untyped_self, next_tag);
    if (res == LF_SLEEP_INTERRUPTED) {
      LF_DEBUG(SCHED, "Sleep interrupted while waiting for STAA");
      continue;
    }
    // Once we are here, we have are committed to executing `next_tag`.

    // If we have reached the stop tag, we break the while loop and go to termination.
    if (lf_tag_compare(next_tag, self->stop_tag) == 0 && going_to_shutdown) {
      break;
    }

    self->prepare_timestep(untyped_self, next_tag);

    Scheduler_pop_events_and_prepare(untyped_self, next_tag);
    LF_DEBUG(SCHED, "Acquired tag %" PRId64 ":%" PRIu32, next_tag.time, next_tag.microstep);

    env->leave_critical_section(env);

    self->run_timestep(untyped_self);
    self->clean_up_timestep(untyped_self);

    env->enter_critical_section(env);
  }

  // Figure out which tag which should execute shutdown at.
  tag_t shutdown_tag;
  if (!non_terminating && self->event_queue.empty(&self->event_queue)) {
    LF_DEBUG(SCHED, "Shutting down due to starvation.");
    shutdown_tag = lf_delay_tag(self->current_tag, 0);
  } else {
    LF_DEBUG(SCHED, "Shutting down because we reached the stop tag.");
    shutdown_tag = self->stop_tag;
  }

  self->super.do_shutdown(untyped_self, shutdown_tag);
}

lf_ret_t Scheduler_schedule_at_locked(Scheduler *untyped_self, Event *event) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  // Check if we are trying to schedule past stop tag
  if (lf_tag_compare(event->tag, self->stop_tag) > 0) {
    LF_WARN(SCHED, "Trying to schedule trigger %p at tag %" PRId64 ":%" PRIu32 " past stop tag %" PRId64 ":%" PRIu32,
            event->trigger, event->tag.time, event->tag.microstep, self->stop_tag.time, self->stop_tag.microstep);
    return LF_AFTER_STOP_TAG;
  }

  // Check if we are tring to schedule into the past
  if (lf_tag_compare(event->tag, self->current_tag) <= 0) {
    LF_WARN(SCHED,
            "Trying to schedule trigger %p at tag %" PRId64 ":%" PRIu32 " which is before current tag %" PRId64
            ":%" PRIu32,
            event->trigger, event->tag.time, event->tag.microstep, self->current_tag.time, self->current_tag.microstep);
    return LF_PAST_TAG;
  }

  lf_ret_t ret = self->event_queue.insert(&self->event_queue, event);
  if (ret != LF_OK) {
    LF_ERR(SCHED, "Failed to insert event into event queue");
  }

  return ret;
}

lf_ret_t Scheduler_schedule_at(Scheduler *self, Event *event) {
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
  LF_INFO(SCHED, "Shutdown requested, will stop at tag %" PRId64 ":%" PRIu32, self->stop_tag.time,
          self->stop_tag.microstep);
  env->platform->new_async_event(env->platform);
  env->leave_critical_section(env);
}

lf_ret_t Scheduler_add_to_reaction_queue(Scheduler *untyped_self, Reaction *reaction) {
  DynamicScheduler *self = (DynamicScheduler *)untyped_self;

  return self->reaction_queue.insert(&self->reaction_queue, reaction);
}

tag_t Scheduler_current_tag(Scheduler *untyped_self) { return ((DynamicScheduler *)untyped_self)->current_tag; }

void DynamicScheduler_ctor(DynamicScheduler *self, Environment *env) {
  self->env = env;

  self->super.keep_alive = false;
  self->super.duration = FOREVER;
  self->super.leader = false;
  self->stop_tag = FOREVER_TAG;
  self->current_tag = NEVER_TAG;
  self->cleanup_ll_head = NULL;
  self->cleanup_ll_tail = NULL;

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
  self->super.acquire_and_schedule_start_tag = Scheduler_acquire_and_schedule_start_tag;
  // self->scheduler.set_duration = Scheduler_set_duration;
  self->super.add_to_reaction_queue = Scheduler_add_to_reaction_queue;
  self->super.current_tag = Scheduler_current_tag;

  EventQueue_ctor(&self->event_queue);
  ReactionQueue_ctor(&self->reaction_queue);
}

Scheduler *Scheduler_new(Environment *env) {
  DynamicScheduler_ctor(&scheduler, env);
  return (Scheduler *)&scheduler;
}
