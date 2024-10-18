#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/scheduler.h"

void Startup_ctor(Startup *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, TRIG_STARTUP, parent, NULL, 0, NULL, NULL, NULL);
  self->effects.reactions = effects;
  self->effects.num_registered = 0;
  self->effects.size = effects_size;
  self->next = NULL;
  parent->register_startup(parent, self);
}

void Shutdown_ctor(Shutdown *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, TRIG_SHUTDOWN, parent, NULL, 0, NULL, NULL, NULL);
  self->effects.reactions = effects;
  self->effects.num_registered = 0;
  self->effects.size = effects_size;
  self->next = NULL;
  parent->register_shutdown(parent, self);
}
