#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/macros.h"
#include <stdio.h>


struct MyStartup {
  Startup super;
  Reaction *effects_[1];
} ;

typedef struct {
  Reaction super;
} MyReactor_0;

struct MyAction {
  LogicalAction super;

  int values[3];
  int value_size;
  int current_value_ready;
  int current_value_write;
};

struct MyReactor {
  Reactor super;
  MyReactor_0 reaction_0;
  struct MyStartup startup;
  struct MyAction my_action;

  Reaction *reactions[1];
  Trigger *triggers[2];
};

typedef struct MyReactor MyReactor;
typedef struct MyAction MyAction;
typedef struct MyStartup MyStartup;


void MyStartup_ctor(MyStartup* self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

void MyAction_ctor(struct MyAction *self, Reactor *parent, Reaction** effects, Reaction** sources) {
  self->value_size = 3;
  self->current_value_ready = 0;
  self->current_value_write = 0;
  LogicalAction_ctor(&self->super, parent, effects, 1, sources, 1);
}

int MyReactor_0_body(Reaction *untyped_reaction) {
  // reactor
  struct MyReactor *self = (struct MyReactor*) untyped_reaction->parent->typed;
  struct MyAction* my_action = &self->my_action;

  // Start User Code

  printf("Hello World\n");
  LogicalAction_schedule(&my_action->super, SEC(1));


  // End User Code
  return 0;
}

int MyReactor_0_calculate_level(Reaction* untyped_self) {
  MyReactor_0 *self = (MyReactor_0 *) (untyped_self->typed);
  struct MyReactor *parent_reactor= (struct MyReactor*) untyped_self->parent->typed;

  int max_level = 0;

  if (self->super.index >= 1) {
    Reaction* reaction_prev_index = parent_reactor->reactions[self->super.index - 1];
    max_level = max(max_level, reaction_prev_index->calculate_level(reaction_prev_index) + 1);
  }

  self->super.level = max_level;

  return max_level;
}

void MyReactor_0_ctor(MyReactor_0 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, MyReactor_0_body, 0, MyReactor_0_calculate_level, self);
}

void MyReactor_ctor(MyReactor* self, Environment *env) {
  // constructing reactor
  Reactor_ctor(&self->super, env, self, NULL, 0, self->reactions, 1, self->triggers, 1);

  // reaction list
  self->reactions[0] = (Reaction *)&self->reaction_0.super;

  // trigger list
  self->triggers[0] = (Trigger *)&self->startup.super.super;

  // constructing reactions
  MyReactor_0_ctor(&self->reaction_0, &self->super);

  // constructing triggers
  MyStartup_ctor(&self->startup, &self->super, &self->reaction_0.super);
  MyAction_ctor(&self->my_action, &self->super, self->reactions, self->reactions);

  // effect configuration
  self->super.register_startup(&self->super, &self->startup.super);
}

int main() {
  Environment env;
  MyReactor my_reactor;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.calculate_levels(&env);
  env.start(&env);
}
