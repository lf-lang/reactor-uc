#include "reactor-uc/reactor-uc.h"
#include "unity.h"
#include <pthread.h>

Environment env;

DEFINE_PHYSICAL_ACTION(MyAction, 1, 1, int, 1);
DEFINE_STARTUP(MyStartup, 1);
DEFINE_SHUTDOWN(MyShutdown, 1);
DEFINE_REACTION(ShutdownReaction, 0);
DEFINE_REACTION(MyReaction, 1);
DEFINE_REACTION(StartupReaction, 1);

typedef struct {
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
} MyReactor;

void MyAction_ctor(MyAction *self, MyReactor *parent) {
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

CONSTRUCT_STARTUP(MyStartup, MyReactor);
CONSTRUCT_SHUTDOWN(MyShutdown, MyReactor)

CONSTRUCT_REACTION(StartupReaction, MyReactor, 0, {
  MyAction *action = &self->my_action;
  pthread_create(&thread, NULL, async_action_scheduler, (void *)action);
});

CONSTRUCT_REACTION(MyReaction, MyReactor, 1, {
  MyAction *my_action = &self->my_action;

  printf("Hello World\n");
  printf("PhysicalAction = %d\n", lf_get(my_action));
  TEST_ASSERT_EQUAL(lf_get(my_action), self->cnt++);
});

CONSTRUCT_REACTION(ShutdownReaction, MyReactor, 2, {
  run_thread = false;
  void *retval;
  int ret = pthread_join(thread, &retval);
});

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[1] = (Reaction *)&self->my_reaction;
  self->_reactions[2] = (Reaction *)&self->shutdown_reaction;
  self->_reactions[0] = (Reaction *)&self->startup_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->my_action;
  self->_triggers[2] = (Trigger *)&self->shutdown;

  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 3, self->_triggers, 3);
  MyAction_ctor(&self->my_action, self);
  MyReaction_ctor(&self->my_reaction, self);
  StartupReaction_ctor(&self->startup_reaction, self);
  ShutdownReaction_ctor(&self->shutdown_reaction, self);
  MyStartup_ctor(&self->startup, self);
  MyShutdown_ctor(&self->shutdown, self);

  STARTUP_REGISTER_EFFECT(self->startup, self->startup_reaction);
  SHUTDOWN_REGISTER_EFFECT(self->shutdown, self->shutdown_reaction);

  ACTION_REGISTER_EFFECT(self->my_action, self->my_reaction);
  REACTION_REGISTER_EFFECT(self->my_reaction, self->my_action);
  ACTION_REGISTER_SOURCE(self->my_action, self->my_reaction);

  self->cnt = 0;
}

void test_simple() {
  MyReactor my_reactor;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.scheduler.set_timeout(&env.scheduler, SEC(1));
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}