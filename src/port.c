#include "reactor-uc/port.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>
#include <string.h>

void Input_prepare(Trigger *_self, Event *event) {
  (void)event;
  assert(_self->type == TRIG_INPUT);
  Input *self = (Input *)_self;
  LF_DEBUG(TRIG, "Preparing input %p with %d effects", self, self->effects.size);
  Scheduler *sched = &self->super.super.parent->env->scheduler;
  _self->is_present = true;
  assert(!_self->is_registered_for_cleanup);
  sched->register_for_cleanup(sched, _self);

  for (size_t i = 0; i < self->effects.size; i++) {
    validaten(sched->reaction_queue.insert(&sched->reaction_queue, self->effects.reactions[i]));
  }
}

void Input_cleanup(Trigger *_self) {
  assert(_self->type == TRIG_INPUT);
  assert(_self->is_registered_for_cleanup);
  LF_DEBUG(TRIG, "Cleaning up input %p", _self);
  _self->is_present = false;
}

void Input_ctor(Input *self, Reactor *parent, Reaction **effects, size_t effects_size, Connection **conns_out,
                size_t conns_out_size, void *value_ptr, size_t value_size) {
  Port_ctor(&self->super, TRIG_INPUT, parent, conns_out, conns_out_size, NULL, 0, effects, effects_size, Input_prepare,
            Input_cleanup);
  self->value_ptr = value_ptr;
  self->value_size = value_size;
}

void Output_ctor(Output *self, Reactor *parent, Reaction **sources, size_t sources_size, Connection **conns_out,
                 size_t conns_out_size) {

  Port_ctor(&self->super, TRIG_OUTPUT, parent, conns_out, conns_out_size, sources, sources_size, NULL, 0, NULL, NULL);
}

void Port_ctor(Port *self, TriggerType type, Reactor *parent, Connection **conns_out, size_t conns_out_size,
               Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
               void (*prepare)(Trigger *, Event *), void (*cleanup)(Trigger *)) {
  Trigger_ctor(&self->super, type, parent, NULL, sources, sources_size, effects, effects_size, prepare, cleanup);
  self->conn_in = NULL;
  self->conns_out = conns_out;
  self->conns_out_size = conns_out_size;
  self->conns_out_registered = 0;
}
