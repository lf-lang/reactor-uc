#include "unity.h"

#define ACTION_LIB_TYPE int
#include "action_lib.h"

DEFINE_REACTION_BODY(ActionLib, reaction) {
  SCOPE_SELF(ActionLib);
  SCOPE_ENV();
  SCOPE_ACTION(ActionLib, act);

  if (self->cnt == 0) {
    TEST_ASSERT_EQUAL(lf_is_present(act), false);
  } else {
    TEST_ASSERT_EQUAL(lf_is_present(act), true);
  }

  printf("Hello World\n");
  printf("Action = %d\n", act->value);
  if (self->cnt > 0) {
    TEST_ASSERT_EQUAL(self->cnt, act->value);
    TEST_ASSERT_EQUAL(self->cnt, env->scheduler.current_tag.microstep);
    TEST_ASSERT_EQUAL(true, lf_is_present(act));
  } else {
    TEST_ASSERT_EQUAL(false, lf_is_present(act));
  }

  TEST_ASSERT_EQUAL(0, env->get_elapsed_logical_time(env));

  if (self->cnt < 100) {
    lf_schedule(act, MSEC(0), ++self->cnt);
  }
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