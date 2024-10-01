#include "reactor-uc/port.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>
#include <string.h>

void Input_prepare(Trigger *_self) {
  assert(_self->type == TRIG_INPUT);
  Input *self = (Input *)_self;
  Scheduler *sched = &self->super.super.parent->env->scheduler;
  self->is_present = true;
  for (size_t i = 0; i < self->effects.size; i++) {
    sched->reaction_queue.insert(&sched->reaction_queue, self->effects.reactions[i]);
  }
}

void Input_cleanup(Trigger *_self) {
  assert(_self->type == TRIG_INPUT);
  Input *self = (Input *)_self;
  self->is_present = false;
}

void Input_ctor(Input *self, Reactor *parent, Reaction **effects, size_t effects_size, void *value_ptr,
                size_t value_size) {
  Port_ctor(&self->super, TRIG_INPUT, parent, Input_prepare, Input_cleanup);
  self->effects.reactions = effects;
  self->effects.num_registered = 0;
  self->effects.size = effects_size;
  self->is_present = false;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
}

void Output_ctor(Output *self, Reactor *parent, Reaction **sources, size_t sources_size) {

  Port_ctor(&self->super, TRIG_OUTPUT, parent, NULL, NULL);
  self->sources.reactions = sources;
  self->sources.size = sources_size;
  self->sources.num_registered = 0;
}

void Port_ctor(Port *self, TriggerType type, Reactor *parent, void (*prepare)(Trigger *), void (*cleanup)(Trigger *)) {
  Trigger_ctor(&self->super, type, parent, prepare, cleanup);
  self->conn_in = NULL;
  self->conn_out = NULL;
}
