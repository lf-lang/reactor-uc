#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>

void Builtin_prepare(Trigger *_self) {
  LF_DEBUG(TRIG, "Preparing builtin trigger %p", _self);
  Scheduler *sched = &_self->parent->env->scheduler;
  TriggerEffects *effects = NULL;
  if (_self->type == TRIG_STARTUP) {
    Startup *self = (Startup *)_self;
    effects = &self->effects;
  } else if (_self->type == TRIG_SHUTDOWN) {
    Shutdown *self = (Shutdown *)_self;
    effects = &self->effects;
  }
  _self->is_present = true;
  sched->register_for_cleanup(sched, _self);

  if (!effects) {
    assert(false);
  } else {
    for (size_t i = 0; i < effects->size; i++) {
      validaten(sched->reaction_queue.insert(&sched->reaction_queue, effects->reactions[i]));
    }
  }
}
void Builtin_cleanup(Trigger *self) {
  LF_DEBUG(TRIG, "Cleaning up builtin trigger %p", self);
  self->is_present = false;
  self->is_registered_for_cleanup = false;
}

void Startup_ctor(Startup *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, TRIG_STARTUP, parent, NULL, Builtin_prepare, Builtin_cleanup, NULL);
  self->effects.reactions = effects;
  self->effects.num_registered = 0;
  self->effects.size = effects_size;
  self->next = NULL;
  parent->register_startup(parent, self);
}

void Shutdown_ctor(Shutdown *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, TRIG_SHUTDOWN, parent, NULL, Builtin_prepare, Builtin_cleanup, NULL);
  self->effects.reactions = effects;
  self->effects.num_registered = 0;
  self->effects.size = effects_size;
  self->next = NULL;
  parent->register_shutdown(parent, self);
}
