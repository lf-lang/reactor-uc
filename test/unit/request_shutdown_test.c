#include "reactor-uc/reactor-uc.h"
#include "unity.h"
#include "action_lib.h"

LF_DEFINE_REACTION_BODY(ActionLib, reaction) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ENV();
  LF_SCOPE_ACTION(ActionLib, act);

  if (env->get_elapsed_logical_time(env) == MSEC(1)) {
    TEST_ASSERT_EQUAL(2, self->cnt);
    env->request_shutdown(env);
  }

  lf_schedule(act, MSEC(1), ++self->cnt);
  lf_schedule(act, MSEC(2), ++self->cnt);
}

LF_DEFINE_REACTION_BODY(ActionLib, r_shutdown) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ENV();

  TEST_ASSERT_EQUAL(4, self->cnt);
  TEST_ASSERT_EQUAL(env->scheduler->start_time + MSEC(1), env->scheduler->current_tag(env->scheduler).time);
  TEST_ASSERT_EQUAL(1, env->scheduler->current_tag(env->scheduler).microstep);
}

void test_run() { lf_start(); }
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_run);
  return UNITY_END();
}