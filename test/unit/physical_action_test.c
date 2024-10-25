#include "reactor-uc/reactor-uc.h"
#include "unity.h"
#include <pthread.h>

Environment env;

DEFINE_ACTION_STRUCT(MyAction, PHYSICAL_ACTION, 1, 1, int, 1)
DEFINE_ACTION_CTOR_FIXED(MyAction, PHYSICAL_ACTION, 1, 1, int, 1, MSEC(0))
DEFINE_STARTUP_STRUCT(MyStartup, 1)
DEFINE_STARTUP_CTOR(MyStartup, 1)
DEFINE_SHUTDOWN_STRUCT(MyShutdown, 1)
DEFINE_SHUTDOWN_CTOR(MyShutdown, 1)
DEFINE_REACTION_STRUCT(MyReactor, 0, 1)
DEFINE_REACTION_STRUCT(MyReactor, 1, 1)
DEFINE_REACTION_STRUCT(MyReactor, 2, 0)

typedef struct {
  Reactor super;
  MyReactor_Reaction0 startup_reaction;
  MyReactor_Reaction1 my_reaction;
  MyReactor_Reaction2 shutdown_reaction;
  MyAction my_action;
  MyStartup startup;
  MyShutdown shutdown;
  Reaction *_reactions[3];
  Trigger *_triggers[3];
  int cnt;
} MyReactor;

bool run_thread = true;
void *async_action_scheduler(void *_action) {
  MyAction *action = (MyAction *)_action;
  int i = 0;
  while (run_thread) {
    env.platform->wait_until(env.platform, env.get_physical_time(&env) + MSEC(1));
    lf_schedule(action, i++, 0);
  }
  return NULL;
}

pthread_t thread;

DEFINE_REACTION_BODY(MyReactor, 0) {
  MyReactor *self = (MyReactor *)_self->parent;
  MyAction *action = &self->my_action;
  pthread_create(&thread, NULL, async_action_scheduler, (void *)action);
};

DEFINE_REACTION_BODY(MyReactor, 1) {
  MyReactor *self = (MyReactor *)_self->parent;
  MyAction *my_action = &self->my_action;

  printf("Hello World\n");
  printf("PhysicalAction = %d\n", my_action->value);
  TEST_ASSERT_EQUAL(my_action->value, self->cnt++);
}

DEFINE_REACTION_BODY(MyReactor, 2) {
  run_thread = false;
  void *retval;
  int ret = pthread_join(thread, &retval);
}

DEFINE_REACTION_CTOR(MyReactor, 0)
DEFINE_REACTION_CTOR(MyReactor, 1)
DEFINE_REACTION_CTOR(MyReactor, 2)

void MyReactor_ctor(MyReactor *self, Environment *_env) {
  self->_reactions[1] = (Reaction *)&self->my_reaction;
  self->_reactions[2] = (Reaction *)&self->shutdown_reaction;
  self->_reactions[0] = (Reaction *)&self->startup_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->my_action;
  self->_triggers[2] = (Trigger *)&self->shutdown;

  Reactor_ctor(&self->super, "MyReactor", _env, NULL, NULL, 0, self->_reactions, 3, self->_triggers, 3);
  MyAction_ctor(&self->my_action, &self->super);
  MyReactor_Reaction0_ctor(&self->startup_reaction, &self->super);
  MyReactor_Reaction1_ctor(&self->my_reaction, &self->super);
  MyReactor_Reaction2_ctor(&self->shutdown_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super);
  MyShutdown_ctor(&self->shutdown, &self->super);

  BUILTIN_REGISTER_EFFECT(self->startup, self->startup_reaction);
  BUILTIN_REGISTER_EFFECT(self->shutdown, self->shutdown_reaction);

  ACTION_REGISTER_EFFECT(self->my_action, self->my_reaction);
  REACTION_REGISTER_EFFECT(self->my_reaction, self->my_action);
  ACTION_REGISTER_SOURCE(self->my_action, self->my_reaction);

  self->cnt = 0;
}

void test_simple() {
  MyReactor my_reactor;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.scheduler.duration = MSEC(100);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}