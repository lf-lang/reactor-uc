#include "reactor-uc/action.h"
#include "reactor-uc/environment.h"

#include <string.h>

static void Action_update_value(Trigger *_self) {
  Action *self = (Action *)_self;
  memcpy(self->value_ptr, self->next_value_ptr, self->value_size);
}

void Action_ctor(Action *self, interval_t min_offset, interval_t min_spacing, Reactor *parent, Reaction **sources,
                 size_t sources_size, Reaction **effects, size_t effects_size, void *value_ptr, void *next_value_ptr,
                 size_t value_size, void (*schedule)(Action *, interval_t)) {
  Trigger_ctor(&self->super, ACTION, parent, effects, effects_size, sources, sources_size, Action_update_value);
  self->value_size = value_size;
  self->value_ptr = value_ptr;
  self->next_value_ptr = next_value_ptr;
  self->min_offset = min_offset;
  self->min_spacing = min_spacing;
  self->previous_event = NEVER_TAG;
  self->schedule = schedule;
}

void LogicalAction_schedule(Action *self, interval_t offset) {
  Environment *env = self->super.parent->env;
  tag_t tag = {.time = env->current_tag.time + self->min_offset + offset, .microstep = 0};
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(tag, earliest_allowed) < 0) {
    assert(false); // TODO: Handle this runtime error
  }

  if (self->super.schedule_at(&self->super, tag) == 0) {
    self->previous_event = tag;
  } else {
    assert(false);
  }
}

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                        Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                        void *value_ptr, void *next_value_ptr, size_t value_size) {
  Action_ctor(&self->super, min_offset, min_spacing, parent, sources, sources_size, effects, effects_size, value_ptr,
              next_value_ptr, value_size, LogicalAction_schedule);
}

void PhysicalAction_schedule(Action *self, interval_t offset) {
  Environment *env = self->super.parent->env;

  env->platform->enter_critical_section(env->platform);

  tag_t tag = {.time = env->get_physical_time(env) + self->min_offset + offset, .microstep = 0};
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(tag, earliest_allowed) < 0) {
    assert(false); // TODO: Handle this runtime error
  }

  if (self->super.schedule_at_locked(&self->super, tag) == 0) {
    self->previous_event = tag;
  } else {
    assert(false);
  }

  env->platform->new_async_event(env->platform);
  env->platform->leave_critical_section(env->platform);
}

void PhysicalAction_ctor(PhysicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                         Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                         void *value_ptr, void *next_value_ptr, size_t value_size) {
  Action_ctor(&self->super, min_offset, min_spacing, parent, sources, sources_size, effects, effects_size, value_ptr,
              next_value_ptr, value_size, PhysicalAction_schedule);
  parent->env->has_physical_action = true;
}
