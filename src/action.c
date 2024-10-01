#include "reactor-uc/action.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/trigger.h"

#include <string.h>

void Action_cleanup(Trigger *self) {
  Action *act = (Action *)self;
  self->is_present = false;
  int ret = act->trigger_value.pop(&act->trigger_value);
  assert(ret == 0);
}

void Action_prepare(Trigger *self) {
  Action *act = (Action *)self;
  Scheduler *sched = &self->parent->env->scheduler;
  self->is_present = true;
  for (size_t i = 0; i < act->effects.size; i++) {
    sched->reaction_queue.insert(&sched->reaction_queue, act->effects.reactions[i]);
  }
}

void Action_ctor(Action *self, TriggerType type, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                 Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size, void *value_buf,
                 size_t value_size, size_t value_capacity, void (*schedule)(Action *, interval_t, const void *)) {
  TriggerValue_ctor(&self->trigger_value, value_buf, value_size, value_capacity);
  SchedulableTrigger_ctor(&self->super, type, parent, &self->trigger_value, Action_prepare, Action_cleanup);
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

void LogicalAction_schedule(Action *self, interval_t offset, const void *value) {
  Environment *env = self->super.super.parent->env;
  tag_t tag = {.time = env->current_tag.time + self->min_offset + offset, .microstep = 0};
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(tag, earliest_allowed) < 0) {
    assert(false); // TODO: Handle this runtime error
  }

  if (self->super.schedule_at(&self->super, tag, value) == 0) {
    self->previous_event = tag;
  } else {
    assert(false);
  }
}

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                        Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                        void *value_buf, size_t value_size, size_t value_capacity) {
  Action_ctor(&self->super, TRIG_LOGICAL_ACTION, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, value_buf, value_size, value_capacity, LogicalAction_schedule);
}

void PhysicalAction_schedule(Action *self, interval_t offset, const void *value) {
  Environment *env = self->super.super.parent->env;

  env->platform->enter_critical_section(env->platform);

  tag_t tag = {.time = env->get_physical_time(env) + self->min_offset + offset, .microstep = 0};
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(tag, earliest_allowed) < 0) {
    assert(false); // TODO: Handle this runtime error
  }

  if (self->super.schedule_at_locked(&self->super, tag, value) == 0) {
    self->previous_event = tag;
  } else {
    assert(false);
  }

  env->platform->new_async_event(env->platform);
  env->platform->leave_critical_section(env->platform);
}

void PhysicalAction_ctor(PhysicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                         Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                         void *value_buf, size_t value_size, size_t value_capacity) {
  Action_ctor(&self->super, TRIG_PHYSICAL_ACTION, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, value_buf, value_size, value_capacity, PhysicalAction_schedule);
  parent->env->has_physical_action = true;
}
