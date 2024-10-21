#include "reactor-uc/action.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/trigger.h"

#include <assert.h>
#include <string.h>

void Action_cleanup(Trigger *self) {
  LF_DEBUG(TRIG, "Cleaning up action %p", self);
  Action *act = (Action *)self;
  self->is_present = false;
  // Actions might not have a type and thus no buffering of payloads.
  if (act->trigger_data_queue.buffer) {
    validaten(act->trigger_data_queue.pop(&act->trigger_data_queue));
  }
}

void Action_prepare(Trigger *self) {
  LF_DEBUG(TRIG, "Preparing action %p", self);
  Action *act = (Action *)self;
  Scheduler *sched = &self->parent->env->scheduler;
  self->is_present = true;

  sched->register_for_cleanup(sched, self);

  for (size_t i = 0; i < act->effects.size; i++) {
    validaten(sched->reaction_queue.insert(&sched->reaction_queue, act->effects.reactions[i]));
  }
}

void Action_ctor(Action *self, TriggerType type, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                 Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size, void *value_buf,
                 size_t value_size, size_t value_capacity, lf_ret_t (*schedule)(Action *, interval_t, const void *)) {
  TriggerDataQueue_ctor(&self->trigger_data_queue, value_buf, value_size, value_capacity);
  Trigger_ctor(&self->super, type, parent, &self->trigger_data_queue, Action_prepare, Action_cleanup, NULL);
  self->min_offset = min_offset;
  self->min_spacing = min_spacing;
  self->previous_event = NEVER_TAG;
  self->schedule = schedule;
  self->sources.reactions = sources;
  self->sources.num_registered = 0;
  self->sources.size = sources_size;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;
}

lf_ret_t LogicalAction_schedule(Action *self, interval_t offset, const void *value) {
  Environment *env = self->super.parent->env;
  Scheduler *sched = &env->scheduler;
  tag_t proposed_tag = lf_delay_tag(sched->current_tag, offset);
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(proposed_tag, earliest_allowed) < 0) {
    return LF_INVALID_TAG;
  }

  if (value) {
    self->trigger_data_queue.stage(&self->trigger_data_queue, value);
    self->trigger_data_queue.push(&self->trigger_data_queue);
  } else {
    return LF_INVALID_VALUE;
  }

  int ret = sched->schedule_at(sched, (Trigger *)self, proposed_tag);
  if (ret == 0) {
    self->previous_event = proposed_tag;
  }
  return ret;
}

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                        Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                        void *value_buf, size_t value_size, size_t value_capacity) {
  Action_ctor(&self->super, TRIG_LOGICAL_ACTION, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, value_buf, value_size, value_capacity, LogicalAction_schedule);
}

lf_ret_t PhysicalAction_schedule(Action *self, interval_t offset, const void *value) {
  LF_DEBUG(TRIG, "Scheduling physical action %p with value %p", self, value);
  Environment *env = self->super.parent->env;
  Scheduler *sched = &env->scheduler;

  env->platform->enter_critical_section(env->platform);

  tag_t tag = {.time = env->get_physical_time(env) + self->min_offset + offset, .microstep = 0};
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(tag, earliest_allowed) < 0) {
    env->platform->leave_critical_section(env->platform);
    return LF_INVALID_TAG;
  }

  // Actions might have no type. Only push data onto the TriggerDataQueue if
  // we have non-null value ptr.
  if (value) {
    assert(self->trigger_data_queue.buffer);
    if (self->trigger_data_queue.stage(&self->trigger_data_queue, value) != LF_OK) {
      env->leave_critical_section(env);
      return LF_OUT_OF_BOUNDS;
    }
    if (self->trigger_data_queue.push(&self->trigger_data_queue) != LF_OK) {
      env->leave_critical_section(env);
      return LF_OUT_OF_BOUNDS;
    }
  }

  int ret = sched->schedule_at_locked(sched, (Trigger *)self, tag);
  if (ret == 0) {
    self->previous_event = tag;
  }

  env->platform->new_async_event(env->platform);
  env->platform->leave_critical_section(env->platform);

  return LF_OK;
}

void PhysicalAction_ctor(PhysicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                         Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                         void *value_buf, size_t value_size, size_t value_capacity) {
  Action_ctor(&self->super, TRIG_PHYSICAL_ACTION, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, value_buf, value_size, value_capacity, PhysicalAction_schedule);
  parent->env->has_async_events = true;
}
