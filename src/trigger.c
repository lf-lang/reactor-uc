#include "reactor-uc/environment.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/trigger.h"

#include <stdio.h>
#include <assert.h>

const void *Trigger_get(Trigger *self) {
  if (self->trigger_value) {
    TriggerValue *tval = self->trigger_value;
    return &tval->buffer[tval->read_idx * tval->value_size];
  } else {
    assert(false);
    return NULL;
  }
}

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, TriggerValue *trigger_value,
                  void (*prepare)(Trigger *), void (*cleanup)(Trigger *), const void *(*get)(Trigger *)) {
  self->type = type;
  self->parent = parent;
  self->next = NULL;
  self->is_present = false;
  self->is_registered_for_cleanup = false;
  self->prepare = prepare;
  self->cleanup = cleanup;
  self->trigger_value = trigger_value;
  if (get) {
    self->get = get;
  } else {
    self->get = Trigger_get;
  }
}
