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
} MyReaction;

struct MyReactor {
  Reactor super;
  MyReaction my_reaction;
  MyAction my_action;
  MyStartup startup;
  Reaction *_reactions[1];
  Trigger *_triggers[2];
  int cnt;
};

void MyStartup_ctor(struct MyStartup *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

int action_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  printf("Hello World\n");
  printf("Action = %d\n", self->my_action.value);
  self->my_action.next_value = self->cnt++;
  tag_t tag = {.time = self->super.env->current_tag.time + 1, .microstep = 0};
  self->my_action.super.schedule_at((Trigger *)&self->my_action.super, tag);
  return 0;
}

void MyReaction_ctor(MyReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, action_handler, self->effects, 1);
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->my_reaction;
  self->_triggers[1] = (Trigger *)&self->my_action;
  Reactor_ctor(&self->super, env, NULL, 0, self->_reactions, 1, self->_triggers, 2);
  MyAction_ctor(&self->my_action, &self->super);
  MyReaction_ctor(&self->my_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super, &self->my_reaction.super);
  self->my_action.super.register_effect(&self->my_action.super, &self->my_reaction.super);
  self->my_reaction.super.register_effect(&self->my_reaction.super, &self->my_action.super);

  self->my_action.super.register_source(&self->my_action.super, &self->my_reaction.super);
  self->super.register_startup(&self->super, &self->startup.super);
}

struct MyReactor my_reactor;
Reactor *reactors[] = {(Reactor *)&my_reactor};
Environment env;

int main() {
  Environment_ctor(&env, reactors, 1);
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
}
