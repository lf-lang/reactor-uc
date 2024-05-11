#include "reactor-uc/action.h"
#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include <stdio.h>

typedef struct MyAction MyAction;
struct MyAction {
  Action_int super;
  Reaction *effects_[1];
  Reaction *sources_[1];
};

typedef struct MyStartup MyStartup;
struct MyStartup {
  Startup super;
  Reaction *effects_[1];
};

void MyStartup_ctor(struct MyStartup *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

struct MyReactor {
  Reactor super;
  Reaction my_reaction;
  MyAction my_action;
  MyStartup startup;
  int cnt;
};

void MyAction_ctor(struct MyAction *self, Reactor *parent, Reaction *effects, Reaction *sources) {
  self->effects_[0] = effects;
  self->sources_[0] = sources;

  Action_int_ctor(&self->super, parent, self->effects_, 1, self->sources_, 1);
}

int action_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  printf("Hello World\n");
  printf("Action = %d\n", self->my_action.super.value);
  self->my_action.super.next_value = self->cnt++;
  tag_t tag = {.time = self->super.env->current_tag.time + 1, .microstep = 0};
  self->my_action.super.super.schedule_at((Trigger *)&self->my_action.super, tag);
  return 0;
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  Reactor_ctor(&self->super, env);
  Reaction_ctor(&self->my_reaction, &self->super, action_handler);
  MyAction_ctor(&self->my_action, &self->super, &self->my_reaction, &self->my_reaction);
  MyStartup_ctor(&self->startup, &self->super, &self->my_reaction);
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
