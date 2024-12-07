
#include "unity.h"

#define ACTION_LIB_TYPE int
#include "action_lib.h"

LF_DEFINE_REACTION_BODY(ActionLib, reaction) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ENV();
  LF_SCOPE_ACTION_EFFECT(ActionLib, act);

  if (self->cnt == 0) {
    TEST_ASSERT_EQUAL(lf_is_present(act), false);
    lf_schedule(act, MSEC(1), 41);
    lf_schedule(act, MSEC(1), 42);
  } else {
    TEST_ASSERT_EQUAL(1, self->cnt);
    TEST_ASSERT_EQUAL(lf_is_present(act), true);
    TEST_ASSERT_EQUAL(42, act->value);
  }
  self->cnt++;
}

LF_DEFINE_REACTION_BODY(ActionLib, r_shutdown) {
}
void test_run() {
  action_lib_start(MSEC(100));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_run);
  return UNITY_END();
}