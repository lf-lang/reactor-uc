#include "reactor-uc/reactor.h"
#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"

#include <string.h>

void Reactor_register_startup(Reactor *self, Startup *startup) {
  (void)self;
  // Startup triggers are registered with the environment as we
  Environment *env = self->env;
  if (!env->startup) {
    tag_t start_tag = {.microstep = 0, .time = self->env->start_time};
    startup->super.schedule_at((Trigger *)startup, start_tag, NULL);
    env->startup = startup;
  } else {
    Trigger *last_in_chain = &env->startup->super;
    while (last_in_chain->next) {
      last_in_chain = last_in_chain->next;
    }
    last_in_chain->next = &startup->super;
  }
}

void Reactor_register_shutdown(Reactor *self, Shutdown *shutdown) {
  (void)self;
  // Startup triggers are registered with the environment as we
  Environment *env = self->env;
  if (!env->shutdown) {
    env->shutdown = shutdown;
  } else {
    Trigger *last_in_chain = &env->shutdown->super;
    while (last_in_chain->next) {
      last_in_chain = last_in_chain->next;
    }
    last_in_chain->next = &shutdown->super;
  }
}

void Reactor_calculate_levels(Reactor *self) {
  for (size_t i = 0; i < self->reactions_size; i++) {
    size_t level = self->reactions[i]->get_level(self->reactions[i]);
    (void)level;
  }

  for (size_t i = 0; i < self->children_size; i++) {
    Reactor_calculate_levels(self->children[i]);
  }
}

void Reactor_ctor(Reactor *self, const char *name, Environment *env, Reactor *parent, Reactor **children,
                  size_t children_size, Reaction **reactions, size_t reactions_size, Trigger **triggers,
                  size_t triggers_size) {
  strncpy(self->name, name, REACTOR_NAME_MAX_LEN);
  self->parent = parent;
  self->env = env;
  self->children = children;
  self->children_size = children_size;
  self->triggers = triggers;
  self->triggers_size = triggers_size;
  self->reactions = reactions;
  self->reactions_size = reactions_size;
  self->register_startup = Reactor_register_startup;
  self->register_shutdown = Reactor_register_shutdown;
  self->calculate_levels = Reactor_calculate_levels;
}
