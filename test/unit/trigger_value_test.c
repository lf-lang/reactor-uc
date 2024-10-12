
#include "unity.h"

#include "reactor-uc/trigger_value.h"

// Verify that we can push a bunch of values and then pop them off
void test_push_pop(void) {
  TriggerValue t;
  int buffer[10];
  TriggerValue_ctor(&t, (char *)&buffer, sizeof(int), 10);

  for (int j = 0; j < 3; j++) {
    int val = 1;
    for (int i = 0; i < 10; i++) {
      int val = j * 10 + i;
      TEST_ASSERT_EQUAL(t.stage(&t, (const void *)&val), 0);
      TEST_ASSERT_EQUAL(t.push(&t), 0);
    }

    for (int i = 0; i < 10; i++) {
      int exp = j * 10 + i;
      TEST_ASSERT_EQUAL(buffer[t.read_idx], exp);
      TEST_ASSERT_EQUAL(t.pop(&t), LF_OK);
    }
  }
}

void test_pop_empty(void) {

  TriggerValue t;
  int buffer[10];
  TriggerValue_ctor(&t, (char *)&buffer, sizeof(int), 10);
  int val = 2;
  t.stage(&t, (const void *)&val);
  t.push(&t);
  TEST_ASSERT_EQUAL(t.pop(&t), LF_OK);
  TEST_ASSERT_EQUAL(t.pop(&t), LF_EMPTY);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_push_pop);
  RUN_TEST(test_pop_empty);
  return UNITY_END();
}