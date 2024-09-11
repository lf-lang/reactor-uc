#include "reactor-uc/reactor.h"
#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"

void register_startup(Reactor *self, Startup *startup) {
  (void)self;
  startup->super.schedule_at((Trigger *)startup, ZERO_TAG);
}

void Reactor_ctor(Reactor *self, Environment *env, void *typed, Reactor **children, size_t children_size,
                  Reaction **reactions, size_t reactions_size, Trigger **triggers, size_t triggers_size) {

  self->env = env;
  self->children = children;
  self->children_size = children_size;
  self->triggers = triggers;
  self->triggers_size = triggers_size;
  self->reactions = reactions;
  self->reactions_size = reactions_size;
  self->register_startup = register_startup;
  self->typed = typed;
}

void Reactor_calculate_levels(Reactor *self) {
  for (size_t i = 0; i < self->children_size; i++) {
    Reactor_calculate_levels(self->children[i]);
  }
  for (size_t i = 0; i < self->reactions_size; i++) {
    self->reactions[i]->calculate_level(self->reactions[i]);
  }
}