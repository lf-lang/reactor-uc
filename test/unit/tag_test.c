#include "reactor-uc/tag.h"
#include "unity.h"

void test_time_add_saturates_positive_overflow(void) {
  TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(FOREVER - 5, 10));
}

void test_time_add_saturates_negative_overflow(void) {
  TEST_ASSERT_EQUAL_INT64(NEVER, lf_time_add(NEVER + 5, -10));
}

void test_time_add_handles_regular_values(void) { TEST_ASSERT_EQUAL_INT64(42, lf_time_add(40, 2)); }

void test_time_add_preserves_sentinels(void) {
  TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(FOREVER, -1));
  TEST_ASSERT_EQUAL_INT64(NEVER, lf_time_add(NEVER, 1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_time_add_saturates_positive_overflow);
  RUN_TEST(test_time_add_saturates_negative_overflow);
  RUN_TEST(test_time_add_handles_regular_values);
  RUN_TEST(test_time_add_preserves_sentinels);
  return UNITY_END();
}
