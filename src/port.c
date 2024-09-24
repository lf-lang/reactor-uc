#include "reactor-uc/environment.h"
#include "reactor-uc/port.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>
#include <string.h>

static void copy_value_down_and_trigger(Port *port, const void *value_ptr, size_t value_size) {
  if (port->conn_out == NULL) {
    return;
  }

  for (size_t i = 0; i < port->conn_out->downstreams_size; i++) {
    Port *down = port->conn_out->downstreams[i];
    if (down->super.type == INPUT) {
      down->super.schedule_now(&down->super, value_ptr);
      ((InputPort *)down)->trigger_effects((InputPort *)down);
    }
    copy_value_down_and_trigger(down, value_ptr, value_size);
  }
}

void InputPort_trigger_effects(InputPort *self) {
  assert(self->super.super.type == INPUT);
  Scheduler *sched = &self->super.super.parent->env->scheduler;
  sched->trigger_reactions(sched, (Trigger *)self);
}

void InputPort_ctor(InputPort *self, Reactor *parent, Reaction **effects, size_t effects_size, size_t value_size,
                    void *value_buf, size_t value_capacity) {
  Port_ctor(&self->super, INPUT, parent, effects, effects_size, NULL, 0, value_size, value_buf, value_capacity);
  self->trigger_effects = InputPort_trigger_effects;
}

void Port_copy_value_and_trigger_downstream(Port *self, const void *value, size_t value_size) {
  copy_value_down_and_trigger(self, value, value_size);
}

void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size) {
  Port_ctor(&self->super, OUTPUT, parent, NULL, 0, sources, sources_size, 0, NULL, 0);
}

void Port_ctor(Port *self, TriggerType type, Reactor *parent, Reaction **effects, size_t effects_size,
               Reaction **sources, size_t sources_size, size_t value_size, void *value_buf, size_t value_capacity) {
  self->conn_in = NULL;
  self->conn_out = NULL;
  TriggerValue_ctor(&self->trigger_value, value_buf, value_size, value_capacity);

  Trigger_ctor(&self->super, type, parent, effects, effects_size, sources, sources_size, &self->trigger_value);

  self->copy_value_and_trigger_downstreams = Port_copy_value_and_trigger_downstream;
}
