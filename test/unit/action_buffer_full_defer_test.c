#include "unity.h"

#define ACTION_LIB_POLICY ACTION_POLICY_DEFER
#define ACTION_LIB_MAX_PENDING 1
#include "action_lib.h"

LF_DEFINE_REACTION_BODY(ActionLib, reaction) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ENV();
  LF_SCOPE_ACTION(ActionLib, act);

  if (self->cnt == 0) {
    // Startup: fill the single-slot buffer, then attempt a second schedule.
    TEST_ASSERT_EQUAL(false, lf_is_present(act));
    TEST_ASSERT_EQUAL(LF_OK, lf_schedule(act, MSEC(2), 41));
    // Buffer is now full. With DEFER policy the second schedule should return LF_VALUE_BUFFER_FULL and the event should
    // be dropped.
    TEST_ASSERT_EQUAL(LF_VALUE_BUFFER_FULL, lf_schedule(act, MSEC(4), 42));
  } else if (self->cnt >= 1) {
    // Only the first event fires at MSEC(2) with value 41.
    TEST_ASSERT_EQUAL(true, lf_is_present(act));
    TEST_ASSERT_EQUAL(env->scheduler->start_time + MSEC(2), env->scheduler->current_tag(env->scheduler).time);
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
