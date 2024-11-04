#include "reactor-uc/trigger.h"
#include "reactor-uc/environment.h"

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, EventPayloadPool *payload_pool, Reaction **sources,
                  size_t sources_size, Reaction **effects, size_t effects_size, void (*prepare)(Trigger *, Event *),
                  void (*cleanup)(Trigger *)) {
  self->type = type;
  self->parent = parent;
  self->next = NULL;
  self->is_present = false;
  self->is_registered_for_cleanup = false;
  self->sources.reactions = sources;
  self->sources.size = sources_size;
  self->sources.num_registered = 0;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;
  self->prepare = prepare;
  self->cleanup = cleanup;
  self->payload_pool = payload_pool;
}
