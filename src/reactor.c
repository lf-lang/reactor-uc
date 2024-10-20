#include "reactor-uc/reactor.h"
#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"

#include <string.h>

/**
 * @brief We only add a single startup event to the event queue, instead
 * chain all startup triggers together.
 */
void Reactor_register_startup(Reactor *self, BuiltinTrigger *startup) {
  (void)self;
  LF_DEBUG(ENV, "Registering startup trigger %p with Reactor %s", startup, self->name);
  Environment *env = self->env;
  Scheduler *sched = &env->scheduler;

  assert(startup->super.type == TRIG_STARTUP);

  if (!env->startup) {
    tag_t start_tag = {.microstep = 0, .time = sched->start_time};
    Event event = EVENT_INIT(start_tag, &startup->super, NULL);
    validaten(env->scheduler.schedule_at(&env->scheduler, &event));
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
  LF_DEBUG(ENV, "Calculating levels for Reactor %s", self->name);
  for (size_t i = 0; i < self->reactions_size; i++) {
    size_t level = self->reactions[i]->get_level(self->reactions[i]);
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
