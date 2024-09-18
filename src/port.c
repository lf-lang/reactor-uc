#include "reactor-uc/environment.h"
#include "reactor-uc/port.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>

void InputPort_trigger_effects(InputPort *self) {
  assert(self->super.super.type == INPUT);
  Scheduler *sched = &self->super.super.parent->env->scheduler;
  for (size_t j = 0; j < self->super.super.effects_size; j++) {
    sched->reaction_queue.insert(&sched->reaction_queue, self->super.super.effects[j]);
  }
}

void InputPort_ctor(InputPort *self, Reactor *parent, Reaction **effects, size_t effects_size, void **value_ptr_ptr) {
  self->value_ptr_ptr = value_ptr_ptr;
  Port_ctor(&self->super, INPUT, parent, effects, effects_size, NULL, 0);
  self->trigger_effects = InputPort_trigger_effects;
}

void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size, void *value_ptr) {
  Port_ctor(&self->super, OUTPUT, parent, NULL, 0, sources, sources_size);
  self->value_ptr = value_ptr;
}

void Port_trigger_downstreams(Port *self) {
  if (!self->conn_out) {
    return;
  }

  for (size_t i = 0; i < self->conn_out->downstreams_size; i++) {
    Port *downstream = self->conn_out->downstreams[i];

    if (downstream->super.type == INPUT) {
      ((InputPort *)downstream)->trigger_effects((InputPort *)downstream);
      downstream->trigger_downstreams(downstream);
    } else {
      Port_trigger_downstreams(downstream);
    }
  }
}

void Port_ctor(Port *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
               Reaction **sources, size_t sources_size) {
  Trigger_ctor(&self->super, type, parent, effects, effects_size, sources, sources_size, NULL);
  self->conn_in = NULL;
  self->conn_out = NULL;

  self->trigger_downstreams = Port_trigger_downstreams;
}
