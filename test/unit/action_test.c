#include "unity.h"

#define ACTION_LIB_TYPE int
#include "action_lib.h"

LF_DEFINE_REACTION_BODY(ActionLib, reaction) {
  LF_SCOPE_SELF(ActionLib);
  LF_SCOPE_ACTION(ActionLib, act);

  if (self->cnt == 0) {
    // First triggering is from startup reaction, and action should be false.
    TEST_ASSERT_EQUAL(lf_is_present(act), false);
  } else {
    // Rest of triggers are from action.
    TEST_ASSERT_EQUAL(lf_is_present(act), true);
  }

  printf("Hello World\n");
  printf("Action = %d\n", act->value);
  if (self->cnt > 0) {
    // The value of the event should be equal to the count.
    TEST_ASSERT_EQUAL(self->cnt, act->value);
  }

  // Schedule count and increment.
  lf_schedule(act, MSEC(1), ++self->cnt);
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