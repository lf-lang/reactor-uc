#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stdio.h>

#define T int
#define N_EFFECTS 1
#define NAME MyAction
#define N_SOURCES 1
#include "reactor-uc/generics/generic_action.h"

typedef struct MyStartup MyStartup;
struct MyStartup {
  Startup super;
  Reaction *effects_[1];
};

typedef struct {
  Reaction super;
  Trigger *effects[1];
} ProducerReaction;

struct ProducerReactor {
  Reactor super;
  ProducerReaction producer_reaction;
  MyAction my_action;
  MyStartup startup;
  Reaction *reactions[1];
  Trigger *triggers[2];
  int cnt;
};

void MyStartup_ctor(struct MyStartup *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

int action_handler(Reaction *_self) {
  struct ProducerReactor *self = (struct ProducerReactor *)_self->parent;
  printf("Hello World\n");
  printf("Action = %d\n", self->my_action.value);
  self->my_action.next_value = self->cnt++;
  tag_t tag = {.time = self->super.env->current_tag.time + 1, .microstep = 0};
  self->my_action.super.schedule_at(&self->my_action.super, tag);
  return 0;
}

void MyReaction_ctor(ProducerReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, action_handler, self->effects, 1);
  self->super.level = 0;
}

void MyReactor_ctor(struct ProducerReactor *self, Environment *env) {
  self->reactions[0] = (Reaction *)&self->producer_reaction;
  self->triggers[0] = (Trigger *)&self->startup;
  self->triggers[1] = (Trigger *)&self->my_action;
  Reactor_ctor(&self->super, env, NULL, 0, self->reactions, 1, self->triggers, 2);
  MyAction_ctor(&self->my_action, &self->super);
  MyReaction_ctor(&self->producer_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super, &self->producer_reaction.super);
  self->my_action.super.register_effect(&self->my_action.super, &self->producer_reaction.super);
  self->producer_reaction.super.register_effect(&self->producer_reaction.super, &self->my_action.super);

  self->my_action.super.register_source(&self->my_action.super, &self->producer_reaction.super);
  self->super.register_startup(&self->super, &self->startup.super);
}

int main() {
  struct ProducerReactor my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
}
