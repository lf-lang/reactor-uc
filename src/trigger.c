#include "reactor-uc/environment.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/util.h"

#include <stdio.h>

int SchedulableTrigger_schedule_at_locked(SchedulableTrigger *self, tag_t tag, const void *value) {
  if (value) {
    assert(self->trigger_value);
    int ret = self->trigger_value->push(self->trigger_value, value);
    assert(ret == 0);
  }

  Event event = {.tag = tag, .trigger = &self->super};
  self->super.parent->env->scheduler.event_queue.insert(&self->super.parent->env->scheduler.event_queue, event);
  return 0;
}

int SchedulableTrigger_schedule_at(SchedulableTrigger *self, tag_t tag, const void *value) {
  Environment *env = self->super.parent->env;

  if (env->has_physical_action) {
    env->platform->enter_critical_section(env->platform);
  }

  int res = self->schedule_at_locked(self, tag, value);

  if (env->has_physical_action) {
    env->platform->leave_critical_section(env->platform);
  }
  return res;
}

void SchedulableTrigger_ctor(SchedulableTrigger *self, TriggerType type, Reactor *parent, TriggerValue *trigger_value,
                             void (*prepare)(Trigger *), void (*cleanup)(Trigger *)) {
  Trigger_ctor(&self->super, type, parent, prepare, cleanup);
  self->trigger_value = trigger_value;
  self->schedule_at = SchedulableTrigger_schedule_at;
  self->schedule_at_locked = SchedulableTrigger_schedule_at_locked;
}

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, void (*prepare)(Trigger *),
                  void (*cleanup)(Trigger *)) {
  self->type = type;
  self->parent = parent;
  self->next = NULL;
  self->is_present = false;
  self->prepare = prepare;
  self->cleanup = cleanup;
}
