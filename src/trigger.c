#include "reactor-uc/trigger.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/util.h"

int Trigger_schedule_at_locked(Trigger *self, tag_t tag, const void *value) {

  if (value) {
    assert(self->trigger_value);
    self->trigger_value->push(self->trigger_value, value);
  }

  Event event = {.tag = tag, .trigger = self};
  self->parent->env->scheduler.event_queue.insert(&self->parent->env->scheduler.event_queue, event);
  self->is_scheduled = true;
  return 0;
}

int Trigger_schedule_at(Trigger *self, tag_t tag, const void *value) {
  Environment *env = self->parent->env;

  if (env->has_physical_action) {
    env->platform->enter_critical_section(env->platform);
  }

  int res = self->schedule_at_locked(self, tag, value);

  if (env->has_physical_action) {
    env->platform->leave_critical_section(env->platform);
  }
  return res;
}

void Trigger_register_effect(Trigger *self, Reaction *reaction) {
  assert(self->effects_registered < self->effects_size);
  self->effects[self->effects_registered++] = reaction;
}

void Trigger_register_source(Trigger *self, Reaction *reaction) {
  assert(self->sources_registered < self->sources_size);
  self->sources[self->sources_registered++] = reaction;
}

void Trigger_prepare(Trigger *self) {
  if (self->trigger_value) {
    self->trigger_value->pop(self->trigger_value);
  }

  if (self->type == TIMER) {
    tag_t next_tag = lf_delay_tag(self->parent->env->current_tag, ((Timer *)self)->period);
    self->schedule_at(self, next_tag, NULL);
  }
}

void Trigger_schedule_now(Trigger *self, const void *value) {
  self->is_present = true;
  self->trigger_value->push(self->trigger_value, value);
  self->prepare(self);
}

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
                  Reaction **sources, size_t sources_size, TriggerValue *trigger_value) {
  self->type = type;
  self->parent = parent;
  self->effects = effects;
  self->effects_size = effects_size;
  self->effects_registered = 0;
  self->sources = sources;
  self->sources_size = sources_size;
  self->sources_registered = 0;
  self->prepare = Trigger_prepare;
  self->next = NULL;
  self->is_present = false;
  self->is_scheduled = false;
  self->trigger_value = trigger_value;

  self->schedule_now = Trigger_schedule_now;
  self->schedule_at = Trigger_schedule_at;
  self->schedule_at_locked = Trigger_schedule_at_locked;
  self->register_effect = Trigger_register_effect;
  self->register_source = Trigger_register_source;
}
