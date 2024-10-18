#include "reactor-uc/trigger.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/timer.h"

#include <assert.h>

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, void *value_ptr, size_t value_size,
                  EventPayloadPool *payload_pool, void (*prepare)(Trigger *, Event *), void (*cleanup)(Trigger *)) {
  self->type = type;
  self->value_ptr = value_ptr;
  self->value_size = value_size;
  self->parent = parent;
  self->next = NULL;
  self->is_present = false;
  self->is_registered_for_cleanup = false;
  self->prepare = prepare;
  self->cleanup = cleanup;
  self->payload_pool = payload_pool;
}
