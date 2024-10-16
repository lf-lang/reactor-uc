#include "reactor-uc/trigger.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/timer.h"

#include <assert.h>

const void *Trigger_get(Trigger *self) {
  if (self->trigger_data_queue) {
    TriggerDataQueue *tval = self->trigger_data_queue;
    return &tval->buffer[tval->read_idx * tval->value_size];
  } else {
    assert(false);
    return NULL;
  }
}

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, TriggerDataQueue *trigger_data_queue,
                  void (*prepare)(Trigger *), void (*cleanup)(Trigger *), const void *(*get)(Trigger *)) {
  self->type = type;
  self->parent = parent;
  self->next = NULL;
  self->is_present = false;
  self->is_registered_for_cleanup = false;
  self->prepare = prepare;
  self->cleanup = cleanup;
  self->trigger_data_queue = trigger_data_queue;
  if (get) {
    self->get = get;
  } else {
    self->get = Trigger_get;
  }
}
