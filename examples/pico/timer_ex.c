#include "reactor-uc/reactor-uc.h"

typedef struct {
  Timer super;
  Reaction *effects[0];
} MyTimer;

typedef struct {
  Reaction super;
} MyReaction;

struct MyReactor {
  Reactor super;
  MyReaction my_reaction;
  MyTimer timer;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
};

void timer_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  printf("Hello World @ %lld\n", self->super.env->current_tag.time);
}

void MyReaction_ctor(MyReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, timer_handler, NULL, 0, 0);
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReaction_ctor(&self->my_reaction, &self->super);
  Timer_ctor(&self->timer.super, &self->super, MSEC(0), MSEC(100), self->timer.effects, 1);
  TIMER_REGISTER_EFFECT(self->timer, self->my_reaction);
}

struct MyReactor my_reactor;
Environment env;
int main() {
  Environment_ctor(&env, (Reactor *)&my_reactor);
  env.set_duration(&env, SEC(1));
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
  return 0;
}
