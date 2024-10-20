#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/scheduler.h"

void Builtin_prepare(Trigger *_self, Event *event) {
  (void)event;
  LF_DEBUG(TRIG, "Preparing builtin trigger %p", _self);
  BuiltinTrigger *self = (BuiltinTrigger *)_self;
  Scheduler *sched = &_self->parent->env->scheduler;
  _self->is_present = true;
  sched->register_for_cleanup(sched, _self);
  assert(self->effects.size > 0);

  for (size_t i = 0; i < self->effects.size; i++) {
    validaten(sched->reaction_queue.insert(&sched->reaction_queue, self->effects.reactions[i]));
  }
}
void Builtin_cleanup(Trigger *self) {
  LF_DEBUG(TRIG, "Cleaning up builtin trigger %p", self);
  self->is_present = false;
  self->is_registered_for_cleanup = false;
}

void BuiltinTrigger_ctor(BuiltinTrigger *self, TriggerType type, Reactor *parent, Reaction **effects,
                         size_t effects_size) {
  Trigger_ctor(&self->super, type, parent, NULL, Builtin_prepare, Builtin_cleanup);
  self->effects.reactions = effects;
  self->effects.num_registered = 0;
  self->effects.size = effects_size;
  self->next = NULL;

  if (type == TRIG_STARTUP) {
    parent->register_startup(parent, self);
  } else if (type == TRIG_SHUTDOWN) {
    parent->register_shutdown(parent, self);
  } else {
    assert(false);
  }
}