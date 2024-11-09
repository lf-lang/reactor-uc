#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_TIMER_STRUCT(TimerTest, t, 1)
DEFINE_TIMER_CTOR(TimerTest, t, 1)
DEFINE_REACTION_STRUCT(TimerTest, reaction, 0)
DEFINE_REACTION_CTOR(TimerTest, reaction, 0)

typedef struct {
  Reactor super;
  REACTION_INSTANCE(TimerTest, reaction);
  TIMER_INSTANCE(TimerTest, t);
  Reaction *_reactions[1];
  Trigger *_triggers[1];
  int cnt;
} TimerTest;

DEFINE_REACTION_BODY(TimerTest, reaction) {
  SCOPE_SELF(TimerTest);
  SCOPE_ENV();
  TEST_ASSERT_EQUAL(self->cnt * MSEC(1), env->get_elapsed_logical_time(env));
  printf("Hello World @ %ld\n", env->get_elapsed_logical_time(env));
  self->cnt++;
}

void TimerTest_ctor(TimerTest *self, Environment *env) {
  Reactor_ctor(&self->super, "TimerTest", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_REACTION(TimerTest, reaction);
  INITIALIZE_TIMER(TimerTest, t, MSEC(0), MSEC(1));
  TIMER_REGISTER_EFFECT(t, reaction);
}

TimerTest my_reactor;
Environment env;
void test_simple() {
  Environment_ctor(&env, (Reactor *)&my_reactor);
  env.scheduler.duration = MSEC(100);
  TimerTest_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}
