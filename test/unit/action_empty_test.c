#include "unity.h"

#define ACTION_LIB_VOID_TYPE
#include "action_lib.h"

DEFINE_REACTION_BODY(ActionLib, reaction) {
  SCOPE_SELF(ActionLib);
  SCOPE_ENV();
  SCOPE_ACTION(ActionLib, act);
  TEST_ASSERT_EQUAL(env->get_elapsed_logical_time(env)/MSEC(1), self->cnt);
  lf_schedule(act, MSEC(1));
  self->cnt++;
}

DEFINE_REACTION_BODY(ActionLib, r_shutdown) {
}


void test_run() {
  action_lib_start(MSEC(100));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_run);
  return UNITY_END();
}