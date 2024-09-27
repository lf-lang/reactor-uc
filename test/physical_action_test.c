#include "reactor-uc/reactor-uc.h"
#include "unity.h"

Environment env;

typedef struct {
  PhysicalAction super;
  int buffer[1];
  Reaction *sources[1];
  Reaction *effects[1];
} MyAction;

typedef struct MyStartup MyStartup;
struct MyStartup {
  Startup super;
  Reaction *effects_[1];
};

typedef struct {
  Shutdown super;
  Reaction *effects_[1];
} MyShutdown;

typedef struct {
  Reaction super;
} ShutdownReaction;

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
  ShutdownReaction shutdown_reaction;
  MyReaction my_reaction;
  MyAction my_action;
  MyStartup startup;
  MyShutdown shutdown;
  Reaction *_reactions[3];
  Trigger *_triggers[3];
  int cnt;
};

void MyAction_ctor(MyAction *self, struct MyReactor *parent) {
  self->sources[0] = &parent->startup_reaction.super;
  self->effects[0] = &parent->my_reaction.super;
  PhysicalAction_ctor(&self->super, MSEC(0), MSEC(0), &parent->super, self->sources, 1, self->effects, 1, self->buffer,
                      sizeof(self->buffer[0]), 2);
}
bool run_thread = true;
void *async_action_scheduler(void *_action) {
  MyAction *action = (MyAction *)_action;
  int i = 0;
  while (run_thread) {
    env.platform->wait_until(env.platform, env.get_physical_time(&env) + MSEC(100));
    lf_schedule(action, i++, 0);
  }
  return NULL;
}

pthread_t thread;
void startup_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  MyAction *action = &self->my_action;
  pthread_create(&thread, NULL, async_action_scheduler, (void *)action);
}

void shutdown_handler(Reaction *_self) {
  (void)_self;
  run_thread = false;
  void *retval;
  int ret = pthread_join(thread, &retval);
}

void MyStartup_ctor(struct MyStartup *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Startup_ctor(&self->super, parent, self->effects_, 1);
}

void MyShutdown_ctor(MyShutdown *self, Reactor *parent, Reaction *effects) {
  self->effects_[0] = effects;
  Shutdown_ctor(&self->super, parent, self->effects_, 1);
}

void StartupReaction_ctor(StartupReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, startup_handler, self->effects, 1, 0);
}

void ShutdownReaction_ctor(ShutdownReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, shutdown_handler, NULL, 0, 2);
}

void action_handler(Reaction *_self) {
  struct MyReactor *self = (struct MyReactor *)_self->parent;
  MyAction *my_action = &self->my_action;

  printf("Hello World\n");
  printf("PhysicalAction = %d\n", lf_get(my_action));
}

void MyReaction_ctor(MyReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, action_handler, self->effects, 1, 1);
}

void MyReactor_ctor(struct MyReactor *self, Environment *env) {
  self->_reactions[1] = (Reaction *)&self->my_reaction;
  self->_reactions[2] = (Reaction *)&self->shutdown_reaction;
  self->_reactions[0] = (Reaction *)&self->startup_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->my_action;
  self->_triggers[2] = (Trigger *)&self->shutdown;

  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 3, self->_triggers, 3);
  MyAction_ctor(&self->my_action, self);
  MyReaction_ctor(&self->my_reaction, &self->super);
  StartupReaction_ctor(&self->startup_reaction, &self->super);
  ShutdownReaction_ctor(&self->shutdown_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super, &self->startup_reaction.super);
  MyShutdown_ctor(&self->shutdown, &self->super, &self->shutdown_reaction.super);

  self->my_action.super.super.super.register_effect(&self->my_action.super.super.super, &self->my_reaction.super);
  self->my_reaction.super.register_effect(&self->my_reaction.super, &self->my_action.super.super.super);

  self->my_action.super.super.super.register_source(&self->my_action.super.super.super, &self->my_reaction.super);
  self->super.register_startup(&self->super, &self->startup.super);
  self->super.register_shutdown(&self->super, &self->shutdown.super);
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