#include "reactor-uc/port.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>
#include <string.h>

static void copy_value_down_and_schedule(Port *port, const void *value_ptr) {
  Environment *env = port->parent->env;
  if (port->conn_out == NULL) {
    return;
  }

  if (port->conn_out->type == CONN_DELAYED) {
    DelayedConnection *conn = (DelayedConnection *)port->conn_out;
    // FIXME: Use proper functions for delaying the schedule time.
    tag_t tag = {.time = env->get_logical_time(env) + conn->delay, .microstep = 0};
    conn->trigger.schedule_at(&conn->trigger, tag, value_ptr);
  } else if (port->conn_out->type == CONN_PHYSICAL) {
    assert(false);
  } else {
    for (size_t i = 0; i < port->conn_out->downstreams_size; i++) {
      Port *down = port->conn_out->downstreams[i];
      if (down->type == INPUT) {
        InputPort *inp = (InputPort *)down;
        memcpy(inp->value_ptr, value_ptr, inp->value_size);
        inp->prepare(inp);
      } else {
        // FIXME: What to do here? Should we copy the value into the output port also?
        assert(false);
      }
      copy_value_down_and_schedule(down, value_ptr);
    }
  }
}

void InputPort_prepare(InputPort *self) {
  assert(self->super.type == INPUT);
  Scheduler *sched = &self->super.parent->env->scheduler;
  self->is_present = true;
  for (size_t i = 0; i < self->effects_size; i++) {
    sched->reaction_queue.insert(&sched->reaction_queue, self->effects[i]);
  }
}

void InputPort_ctor(InputPort *self, Reactor *parent, Reaction **effects, size_t effects_size, void *value_ptr,
                    size_t value_size) {
  Port_ctor(&self->super, INPUT, parent);
  self->effects = effects;
  self->effects_registered = 0;
  self->effects_size = effects_size;
  self->is_present = false;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
  self->prepare = InputPort_prepare;
}

// FIXME: Rethink API here, do we need the value-ptr if we store the value in all ports?
void Port_copy_value_and_shedule_downstream(Port *self, const void *value) {
  copy_value_down_and_schedule(self, value);
}

void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **sources, size_t sources_size, void *value_ptr,
                     size_t value_size) {

  Port_ctor(&self->super, OUTPUT, parent);
  self->is_present = false;
  self->sources = sources;
  self->sources_size = sources_size;
  self->sources_registered = 0;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
}

void Port_ctor(Port *self, PortType type, Reactor *parent) {
  self->type = type;
  self->parent = parent;
  self->conn_in = NULL;
  self->conn_out = NULL;
  self->copy_value_and_schedule_downstreams = Port_copy_value_and_shedule_downstream;
}
