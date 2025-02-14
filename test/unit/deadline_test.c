#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#include <unistd.h>

LF_DEFINE_TIMER_STRUCT(TimerTest, t, 1, 0)
LF_DEFINE_REACTION_STRUCT(TimerTest, reaction, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(TimerTest, reaction);
  LF_TIMER_INSTANCE(TimerTest, t);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
  int cnt;
} TimerTest;

LF_DEFINE_REACTION_BODY(TimerTest, reaction) {
  LF_SCOPE_SELF(TimerTest);
  LF_SCOPE_ENV();
  TEST_ASSERT_EQUAL(self->cnt * MSEC(1), env->get_elapsed_logical_time(env));
  printf("Hello World @ %ld\n", env->get_elapsed_logical_time(env));
  self->cnt++;
  sleep(self->cnt);
}

LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(TimerTest, reaction) {
  LF_SCOPE_SELF(TimerTest);
  LF_SCOPE_ENV();

  TEST_ASSERT(self->cnt == 2);
  printf("Deadline Violated!!!\n");
  env->request_shutdown(env);
}

LF_DEFINE_TIMER_CTOR(TimerTest, t, 1, 0)
LF_DEFINE_REACTION_CTOR(TimerTest, reaction, 0, LF_REACTION_TYPE(TimerTest, reaction_deadline_violation_handler),
                        SEC(2), NULL)

LF_REACTOR_CTOR_SIGNATURE(TimerTest) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(TimerTest);
  LF_INITIALIZE_REACTION(TimerTest, reaction);
  LF_INITIALIZE_TIMER(TimerTest, t, MSEC(0), MSEC(1));
  LF_TIMER_REGISTER_EFFECT(self->t, self->reaction);
}

LF_ENTRY_POINT(TimerTest, 32, 32, MSEC(100), false, false);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}
