#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stdio.h>

typedef struct {
  Startup super;
  Reaction *effects_[1];
} MyStartup;

typedef struct {
  Reaction super;
} MyReaction;

typedef struct {
  Reactor super;
  MyReaction my_reaction;
  MyStartup startup;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
  int cnt;
} MyReactor;

void MyStartup_ctor(MyStartup *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

int startup_handler(Reaction *_self) {
  MyReactor *self = (MyReactor *)_self->parent;
  (void)self;
  printf("Hello World\n");
  return 0;
}

void MyReaction_ctor(MyReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, startup_handler, NULL, 0);
  self->super.level = 0;
}

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  Reactor_ctor(&self->super, env, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReaction_ctor(&self->my_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super, &self->my_reaction.super);
  self->super.register_startup(&self->super, &self->startup.super);
}

int main() {
  Environment env;
  MyReactor my_reactor;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
}
