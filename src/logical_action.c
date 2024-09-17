#include "reactor-uc/logical_action.h"
#include <string.h>

static void LogicalAction_update_value(Trigger *_self) {
  LogicalAction *self = (LogicalAction *)_self;
  memcpy(self->value_ptr, self->next_value_ptr, self->value_size);
}

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, Reactor *parent, Reaction **sources,
                        size_t sources_size, Reaction **effects, size_t effects_size, void *value_ptr,
                        void *next_value_ptr, size_t value_size) {
  Trigger_ctor(&self->super, ACTION, parent, effects, effects_size, sources, sources_size, LogicalAction_update_value);
  self->value_size = value_size;
  self->value_ptr = value_ptr;
  self->next_value_ptr = next_value_ptr;
  self->min_offset = min_offset;
}
