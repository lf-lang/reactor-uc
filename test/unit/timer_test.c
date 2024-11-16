#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_TIMER_STRUCT(TimerTest, t, 1, 0)
DEFINE_TIMER_CTOR(TimerTest, t, 1, 0)
DEFINE_REACTION_STRUCT(TimerTest, reaction, 0)
DEFINE_REACTION_CTOR(TimerTest, reaction, 0)

typedef struct {
  Reactor super;
  REACTION_INSTANCE(TimerTest, reaction);
  TIMER_INSTANCE(TimerTest, t);
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
  int cnt;
} TimerTest;

DEFINE_REACTION_BODY(TimerTest, reaction) {
  SCOPE_SELF(TimerTest);
  SCOPE_ENV();
  TEST_ASSERT_EQUAL(self->cnt * MSEC(1), env->get_elapsed_logical_time(env));
  printf("Hello World @ %ld\n", env->get_elapsed_logical_time(env));
  self->cnt++;
}

REACTOR_CTOR_SIGNATURE(TimerTest) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(TimerTest);
  INITIALIZE_REACTION(TimerTest, reaction);
  INITIALIZE_TIMER(TimerTest, t, MSEC(0), MSEC(1));
  TIMER_REGISTER_EFFECT(t, reaction);
}

TimerTest my_reactor;
Environment env;
void test_simple() {
  Environment_ctor(&env, (Reactor *)&my_reactor);
  env.scheduler.duration = MSEC(100);
  TimerTest_ctor(&my_reactor, NULL, &env);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}
