#include "reactor-uc/environment.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

#include <stdio.h>

void Trigger_cleanup(Trigger *self) {
  self->is_present = false;
  if (self->trigger_value) {
    int ret = self->trigger_value->pop(self->trigger_value);
    assert(ret == 0);
  }
}

void Trigger_prepare(Trigger *self) {
  Scheduler *sched = &self->parent->env->scheduler;
  self->is_present = true;
  for (size_t i = 0; i < self->effects_size; i++) {
    sched->reaction_queue.insert(&sched->reaction_queue, self->effects[i]);
  }
}

int Trigger_schedule_at_locked(Trigger *self, tag_t tag, const void *value) {
  if (value) {
    assert(self->trigger_value);
    int ret = self->trigger_value->push(self->trigger_value, value);
    assert(ret == 0);
  }

  Event event = {.tag = tag, .trigger = self};
  self->parent->env->scheduler.event_queue.insert(&self->parent->env->scheduler.event_queue, event);
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

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
                  Reaction **sources, size_t sources_size, TriggerValue *trigger_value, void *friend,
                  void (*prepare)(Trigger *), void (*cleanup)(Trigger *)) {
  self->type = type;
  self->friend = friend;
  self->parent = parent;
  self->effects = effects;
  self->effects_size = effects_size;
  self->effects_registered = 0;
  self->sources = sources;
  self->sources_size = sources_size;
  self->sources_registered = 0;
  self->next = NULL;
  self->is_present = false;
  self->trigger_value = trigger_value;

  if (prepare) {
    self->prepare = prepare;
  } else {
    self->prepare = Trigger_prepare;
  }
  if (cleanup) {
    self->cleanup = cleanup;
  } else {
    self->cleanup = Trigger_cleanup
  }
  self->schedule_at_locked = Trigger_schedule_at_locked;
  self->register_effect = Trigger_register_effect;
  self->register_source = Trigger_register_source;
}
