#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stdio.h>

typedef struct MyStartup MyStartup;
struct MyStartup {
  Startup super;
  Reaction *effects_[1];
};

struct MyReactor {
  Reactor super;
  Reaction my_reaction;
  MyStartup startup;
  int cnt;
};

void MyStartup_ctor(struct MyStartup *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

int startup_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  (void)self;
  printf("Hello World\n");
  return 0;
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  Reactor_ctor(&self->super, env);
  Reaction_ctor(&self->my_reaction, &self->super, startup_handler);
  MyStartup_ctor(&self->startup, &self->super, &self->my_reaction);
  self->super.register_startup(&self->super, &self->startup.super);
}

struct MyReactor my_reactor;
Reactor *reactors[] = {(Reactor *)&my_reactor};

int main() {
  Environment env;
  Environment_ctor(&env, reactors, 1);
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
}
