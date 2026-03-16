#include "unity.h"

#define ACTION_LIB_POLICY drop
#define ACTION_LIB_MIN_SPACING MSEC(5)
#include "action_lib.h"

LF_DEFINE_REACTION_BODY(ActionLib, reaction) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ENV();
  LF_SCOPE_ACTION(ActionLib, act);

  if (self->cnt == 0) {
    // Startup: schedule two events that will violate min_spacing
    TEST_ASSERT_EQUAL(false, lf_is_present(act));
    lf_schedule(act, MSEC(1), 41);
    lf_schedule(act, MSEC(2), 42); // Should be dropped
  } else if (self->cnt >= 1) {
    // Only the first event fires at MSEC(1) with value 41
    TEST_ASSERT_EQUAL(true, lf_is_present(act));
    TEST_ASSERT_EQUAL(env->scheduler->start_time + MSEC(1),
                      env->scheduler->current_tag(env->scheduler).time);
    TEST_ASSERT_EQUAL(41, act->value);
  }
  self->cnt++;
}

LF_DEFINE_REACTION_BODY(ActionLib, r_shutdown) {
  LF_SCOPE_SELF(ActionLib);
  TEST_ASSERT_EQUAL(2, self->cnt);
}

void test_run() { lf_start(); }
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_run);
  return UNITY_END();
}
