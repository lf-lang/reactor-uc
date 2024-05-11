#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reactor.h"

void register_startup(Reactor *self, Startup *startup) {
  (void)self;
  startup->super.schedule_at((Trigger *)startup, ZERO_TAG);
}

void Reactor_ctor(Reactor *self, Environment *env) {
  self->env = env;
  self->register_startup = register_startup;
}
