#include "reactor-uc/trigger.h"
#include "reactor-uc/environment.h"

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, EventPayloadPool *payload_pool,
                  void (*prepare)(Trigger *, Event *), void (*cleanup)(Trigger *)) {
  self->type = type;
  self->parent = parent;
  self->next = NULL;
  self->is_present = false;
  self->is_registered_for_cleanup = false;
  self->prepare = prepare;
  self->cleanup = cleanup;
  self->payload_pool = payload_pool;
}
