#include "unity.h"

#define ACTION_LIB_POLICY update
#define ACTION_LIB_MIN_SPACING MSEC(5)
#include "action_lib.h"

LF_DEFINE_REACTION_BODY(ActionLib, reaction) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ENV();
  LF_SCOPE_ACTION(ActionLib, act);
  printf("Reaction executed at time " PRINTF_TIME ", with value %i\n", env->scheduler->current_tag(env->scheduler).time,
         act->value);

  if (self->cnt == 0) {
    // Startup: schedule two events that will violate min_spacing
    TEST_ASSERT_EQUAL(false, lf_is_present(act));
    lf_schedule(act, MSEC(1), 41);
    lf_schedule(act, MSEC(2), 42); // Should cancel old, schedule at MSEC(2)
  } else if (self->cnt >= 1) {
    // Event fires at MSEC(2) with value 42 (old event cancelled)
    TEST_ASSERT_EQUAL(true, lf_is_present(act));
    TEST_ASSERT_EQUAL(env->scheduler->start_time + MSEC(2), env->scheduler->current_tag(env->scheduler).time);
    TEST_ASSERT_EQUAL(42, act->value);
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
