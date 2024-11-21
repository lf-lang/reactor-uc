#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"
#include <pthread.h>

Environment env;

DEFINE_ACTION_STRUCT(PhyActionTest, act, PHYSICAL_ACTION, 1, 1, 1, int);
DEFINE_ACTION_CTOR(PhyActionTest, act, PHYSICAL_ACTION, 1, 1, 1, int);
DEFINE_STARTUP_STRUCT(PhyActionTest, 1);
DEFINE_STARTUP_CTOR(PhyActionTest);
DEFINE_SHUTDOWN_STRUCT(PhyActionTest, 1);
DEFINE_SHUTDOWN_CTOR(PhyActionTest);

DEFINE_REACTION_STRUCT(PhyActionTest, r_startup, 1);
DEFINE_REACTION_STRUCT(PhyActionTest, r_action, 1);
DEFINE_REACTION_STRUCT(PhyActionTest, r_shutdown, 0);

DEFINE_REACTION_CTOR(PhyActionTest, r_startup, 0);
DEFINE_REACTION_CTOR(PhyActionTest, r_action, 1);
DEFINE_REACTION_CTOR(PhyActionTest, r_shutdown ,2);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(PhyActionTest, r_startup);
  REACTION_INSTANCE(PhyActionTest, r_action);
  REACTION_INSTANCE(PhyActionTest, r_shutdown);
  ACTION_INSTANCE(PhyActionTest, act);
  STARTUP_INSTANCE(PhyActionTest);
  SHUTDOWN_INSTANCE(PhyActionTest);
  REACTOR_BOOKKEEPING_INSTANCES(3,3,0);
  int cnt;
} PhyActionTest;

bool run_thread = true;
void *async_action_scheduler(void *_action) {
  PhyActionTest_act *action = (PhyActionTest_act *)_action;
  int i = 0;
  while (run_thread) {
    env.platform->wait_until(env.platform, env.get_physical_time(&env) + MSEC(1));
    lf_schedule(action, 0, i++);
  }
  return NULL;
}

pthread_t thread;

DEFINE_REACTION_BODY(PhyActionTest, r_startup) {
  SCOPE_SELF(PhyActionTest);
  SCOPE_ACTION(PhyActionTest, act);
  pthread_create(&thread, NULL, async_action_scheduler, (void *)act);
};

DEFINE_REACTION_BODY(PhyActionTest, r_action) {
  SCOPE_SELF(PhyActionTest);
  SCOPE_ACTION(PhyActionTest, act);

  printf("Hello World\n");
  printf("PhysicalAction = %d\n", act->value);
  TEST_ASSERT_EQUAL(act->value, self->cnt++);
}

DEFINE_REACTION_BODY(PhyActionTest, r_shutdown) {
  run_thread = false;
  void *retval;
  int ret = pthread_join(thread, &retval);
}


REACTOR_CTOR_SIGNATURE(PhyActionTest) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(PhyActionTest);

  INITIALIZE_REACTION(PhyActionTest, r_startup);
  INITIALIZE_REACTION(PhyActionTest, r_action);
  INITIALIZE_REACTION(PhyActionTest, r_shutdown);
  INITIALIZE_ACTION(PhyActionTest, act, MSEC(0));
  INITIALIZE_STARTUP(PhyActionTest);
  INITIALIZE_SHUTDOWN(PhyActionTest);

  STARTUP_REGISTER_EFFECT(r_startup);
  SHUTDOWN_REGISTER_EFFECT(r_shutdown);

  ACTION_REGISTER_EFFECT(act, r_action);
  REACTION_REGISTER_EFFECT(r_startup, act);
  REACTION_REGISTER_EFFECT(r_action, act);
  ACTION_REGISTER_SOURCE(act, r_action);

  self->cnt = 0;
}

void test_simple() {
  PhyActionTest my_reactor;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  PhyActionTest_ctor(&my_reactor, NULL, &env);
  env.scheduler->duration = MSEC(100);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}