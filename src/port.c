#include "reactor-uc/port.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/scheduler.h"
#include <assert.h>
#include <string.h>

void Port_prepare(Trigger *_self, Event *event) {
  (void)event;
  assert(_self->type == TRIG_INPUT || _self->type == TRIG_OUTPUT);

  Port *self = (Port *)_self;
  LF_DEBUG(TRIG, "Preparing port %p with %d effects", self, self->effects.size);
  Scheduler *sched = self->super.parent->env->scheduler;
  _self->is_present = true;
  assert(!_self->is_registered_for_cleanup);
  sched->register_for_cleanup(sched, _self);

  for (size_t i = 0; i < self->effects.size; i++) {
    validaten(sched->add_to_reaction_queue(sched, self->effects.reactions[i]));
  }
}

void Port_set(Port *self, const void *value) {

  if (self->effects.size > 0 || self->observers.size > 0) {
    memcpy(self->value_ptr, value, self->value_size);
    Port_prepare(&self->super, NULL);
  }

  for (size_t i = 0; i < self->conns_out_registered; i++) {
    Connection *conn = self->conns_out[i];
    conn->trigger_downstreams(conn, value, self->value_size);
  }
}

void Port_cleanup(Trigger *_self) {
  assert(_self->type == TRIG_INPUT || _self->type == TRIG_OUTPUT);
  assert(_self->is_registered_for_cleanup);
  LF_DEBUG(TRIG, "Cleaning up port %p", _self);
  _self->is_present = false;
}

void Port_ctor(Port *self, TriggerType type, Reactor *parent, void *value_ptr, size_t value_size, Reaction **effects,
               size_t effects_size, Reaction **sources, size_t sources_size, Reaction **observers,
               size_t observers_size, Connection **conns_out, size_t conns_out_size) {
  Trigger_ctor(&self->super, type, parent, NULL, Port_prepare, Port_cleanup);
  self->set = Port_set;
  self->conn_in = NULL;
  self->conns_out = conns_out;
  self->conns_out_size = conns_out_size;
  self->conns_out_registered = 0;
  self->sources.reactions = sources;
  self->sources.size = sources_size;
  self->sources.num_registered = 0;
  self->effects.reactions = effects;
  self->effects.num_registered = 0;
  self->effects.size = effects_size;
  self->observers.reactions = observers;
  self->observers.size = observers_size;
  self->observers.num_registered = 0;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
}
