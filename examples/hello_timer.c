#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/timer.h"
#include <stdio.h>

typedef struct {
  Timer super;
  Reaction *effects[1];
} MyTimer;

struct MyReactor {
  Reactor super;
  Reaction my_reaction;
  MyTimer timer;
};

int timer_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  printf("Hello World @ %ld\n", self->super.env->current_tag.time);
  return 0;
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  Reactor_ctor(&self->super, env);
  Reaction_ctor(&self->my_reaction, &self->super, timer_handler);
  Timer_ctor(&self->timer.super, &self->super, 0, SEC(1), self->timer.effects, 1);
  self->timer.super.super.register_effect(&self->timer.super.super, &self->my_reaction);
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
