#include "reactor-uc/reactor-uc.h"
#include "unity.h"

Environment env;

typedef struct {
  PhysicalAction super;
  int value;
  int next_value;
  Reaction *sources[1];
  Reaction *effects[1];
} MyAction;

typedef struct MyStartup MyStartup;
struct MyStartup {
  Startup super;
  Reaction *effects_[1];
};

typedef struct {
  Reaction super;
  Trigger *effects[1];
} MyReaction;

typedef struct {
  Reaction super;
  Trigger *effects[1];
} StartupReaction;

struct MyReactor {
  Reactor super;
  StartupReaction startup_reaction;
  MyReaction my_reaction;
  MyAction my_action;
  MyStartup startup;
  Reaction *_reactions[2];
  Trigger *_triggers[2];
  int cnt;
};

void MyAction_ctor(MyAction *self, struct MyReactor *parent) {
  self->sources[0] = &parent->startup_reaction.super;
  self->effects[0] = &parent->my_reaction.super;
  PhysicalAction_ctor(&self->super, MSEC(0), MSEC(0), &parent->super, self->sources, 1, self->effects, 1, &self->value,
                      &self->next_value, sizeof(self->value));
}

void *async_action_scheduler(void *_action) {
  MyAction *action = (MyAction *)_action;
  int i = 0;
  while (true) {
    env.platform->wait_until(env.platform, env.get_physical_time(&env) + MSEC(100));
    lf_schedule(action, i++, 0);
  }
}

int startup_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  MyAction *action = &self->my_action;
  pthread_t thread;
  pthread_create(&thread, NULL, async_action_scheduler, (void *)action);
}

void MyStartup_ctor(struct MyStartup *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

void StartupReaction_ctor(StartupReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, startup_handler, self->effects, 1, 0);
}

// FIXME: Why is this called even when I did not trigger it?
int action_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  MyAction *my_action = &self->my_action;

  printf("Hello World\n");
  printf("PhysicalAction = %d\n", my_action->value);
  return 0;
}

void MyReaction_ctor(MyReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, action_handler, self->effects, 1, 1);
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  self->_reactions[1] = (Reaction *)&self->my_reaction;
  self->_reactions[0] = (Reaction *)&self->startup_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->my_action;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, 0, self->_reactions, 2, self->_triggers, 2);
  MyAction_ctor(&self->my_action, self);
  MyReaction_ctor(&self->my_reaction, &self->super);
  StartupReaction_ctor(&self->startup_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super, &self->startup_reaction.super);
  self->my_action.super.super.super.register_effect(&self->my_action.super.super.super, &self->my_reaction.super);
  self->my_reaction.super.register_effect(&self->my_reaction.super, &self->my_action.super.super.super);

  self->my_action.super.super.super.register_source(&self->my_action.super.super.super, &self->my_reaction.super);
  self->super.register_startup(&self->super, &self->startup.super);
  self->cnt = 0;
}

void test_simple() {
  struct MyReactor my_reactor;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.set_stop_time(&env, SEC(1));
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}