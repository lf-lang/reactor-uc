#include "unity.h"

#define ACTION_LIB_VOID_TYPE
#include "action_lib.h"

LF_DEFINE_REACTION_BODY(ActionLib, reaction) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ENV();
  LF_SCOPE_ACTION(ActionLib, act);
  TEST_ASSERT_EQUAL(env->get_elapsed_logical_time(env) / MSEC(1), self->cnt);
  lf_schedule(act, MSEC(1));
  self->cnt++;
}

LF_DEFINE_REACTION_BODY(ActionLib, r_shutdown) {
}

void test_run() {
  lf_start();
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_run);
  return UNITY_END();
}