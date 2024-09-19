#include "reactor-uc/environment.h"
#include "reactor-uc/logical_action.h"

#include <string.h>

static void LogicalAction_update_value(Trigger *_self) {
  LogicalAction *self = (LogicalAction *)_self;
  memcpy(self->value_ptr, self->next_value_ptr, self->value_size);
}

void LogicalAction_schedule(LogicalAction *self, interval_t offset) {
  Environment *env = self->super.parent->env;
  tag_t tag = {.time = env->current_tag.time + self->min_offset + offset, .microstep = 0};
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(tag, earliest_allowed) < 0) {
    assert(false); // TODO: Handle this runtime error
  }

  self->super.schedule_at(&self->super, tag);
}

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                        Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                        void *value_ptr, void *next_value_ptr, size_t value_size) {
  Trigger_ctor(&self->super, ACTION, parent, effects, effects_size, sources, sources_size, LogicalAction_update_value);
  self->value_size = value_size;
  self->value_ptr = value_ptr;
  self->next_value_ptr = next_value_ptr;
  self->min_offset = min_offset;
  self->min_spacing = min_spacing;
  self->previous_event = NEVER_TAG;
  self->schedule = LogicalAction_schedule;
}
