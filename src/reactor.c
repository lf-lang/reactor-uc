#include "reactor-uc/reactor.h"
#include "reactor-uc/port.h"
#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"

#include <string.h>

void Reactor_validate(Reactor *self) {
  validate(self->env);
  int prev_level = -1;
  for (size_t i = 0; i < self->reactions_size; i++) {
    Reaction *reaction = self->reactions[i];
    validate(reaction);
    validate(reaction->index == i);
    validate(reaction->parent == self);
    validate(reaction->level >= 0);
    validate(reaction->level > prev_level);
    validate(reaction->effects_size == reaction->effects_registered);
    for (size_t i = 0; i < reaction->effects_size; i++) {
      Trigger *effect = reaction->effects[i];
      validate(effect);
    }
    prev_level = reaction->level;
  }
  for (size_t i = 0; i < self->triggers_size; i++) {
    Trigger *trigger = self->triggers[i];
    validate(trigger);

    if (trigger->type == TRIG_INPUT || trigger->type == TRIG_OUTPUT) {
      Port *port = (Port *)trigger;
      validate(port->effects.num_registered == port->effects.size);
      validate(port->sources.num_registered == port->sources.size);
      validate(port->conns_out_size >= port->conns_out_registered);
      for (size_t i = 0; i < port->conns_out_registered; i++) {
        Connection *conn = port->conns_out[i];
        validate(conn);
        validate(conn->upstream == port);
      }
    }
  }
  for (size_t i = 0; i < self->children_size; i++) {
    validate(self->children[i]);
    Reactor_validate(self->children[i]);
  }
}
/**
 * @brief We only add a single startup event to the event queue, instead
 * chain all startup triggers together.
 */
void Reactor_register_startup(Reactor *self, BuiltinTrigger *startup) {
  (void)self;
  LF_DEBUG(ENV, "Registering startup trigger %p with Reactor %s", startup, self->name);
  Environment *env = self->env;
  if (!env->startup) {
    env->startup = startup;
  } else {
    BuiltinTrigger *last_in_chain = env->startup;
    while (last_in_chain->next) {
      assert(last_in_chain->super.type == TRIG_STARTUP);
      last_in_chain = last_in_chain->next;
    }
    last_in_chain->next = startup;
  }
}

/**
 * @brief We do not put any shutdown event on the event queue. Instead
 * we chain the shutdown triggers together and handle them when we arrive
 * at the timeout, or due to starvation.
 */
void Reactor_register_shutdown(Reactor *self, BuiltinTrigger *shutdown) {
  (void)self;
  LF_DEBUG(ENV, "Registering shutdown trigger %p with Reactor %s", shutdown, self->name);
  Environment *env = self->env;

  assert(shutdown->super.type == TRIG_SHUTDOWN);
  if (!env->shutdown) {
    env->shutdown = shutdown;
  } else {
    BuiltinTrigger *last_in_chain = env->shutdown;
    while (last_in_chain->next) {
      assert(last_in_chain->super.type == TRIG_SHUTDOWN);
      last_in_chain = last_in_chain->next;
    }
    last_in_chain->next = shutdown;
  }
}

lf_ret_t Reactor_calculate_levels(Reactor *self) {
  validate(self);
  LF_DEBUG(ENV, "Calculating levels for Reactor %s", self->name);
  for (size_t i = 0; i < self->reactions_size; i++) {
    size_t level = self->reactions[i]->get_level(self->reactions[i]);
    LF_DEBUG(ENV, "Reaction %d has level %d", i, level);
    (void)level;
  }

  for (size_t i = 0; i < self->children_size; i++) {
    int res = Reactor_calculate_levels(self->children[i]);
    if (res != LF_OK) {
      return res;
    }
  }
  return LF_OK;
}

void Reactor_ctor(Reactor *self, const char *name, Environment *env, Reactor *parent, Reactor **children,
                  size_t children_size, Reaction **reactions, size_t reactions_size, Trigger **triggers,
                  size_t triggers_size) {
  strncpy(self->name, name, REACTOR_NAME_MAX_LEN - 1); // NOLINT
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
