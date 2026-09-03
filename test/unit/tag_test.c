#include "unity.h"

#include "reactor-uc/tag.h"

/**
 * The time-unit macros must parenthesize their argument, otherwise any
 * additive or bitwise expression binds tighter than the multiplication.
 */
void test_time_unit_macros_parenthesize_argument(void) {
  TEST_ASSERT_EQUAL_INT64(3000000000LL, SEC(1 + 2));
  TEST_ASSERT_EQUAL_INT64(1000000LL, MSEC(2 - 1));
  TEST_ASSERT_EQUAL_INT64(2000LL, USEC(1 + 1));
  TEST_ASSERT_EQUAL_INT64(3LL, NSEC(1 + 2));
  TEST_ASSERT_EQUAL_INT64(120000000000LL, MINS(1 + 1));
  TEST_ASSERT_EQUAL_INT64(7200000000000LL, HOURS(1 + 1));
  TEST_ASSERT_EQUAL_INT64(172800000000000LL, DAYS(1 + 1));
  TEST_ASSERT_EQUAL_INT64(1209600000000000LL, WEEKS(1 + 1));
}

/**
 * tag.h documents lf_delay_strict as returning the tag unmodified when the
 * interval is negative. A returned tag must never be strictly earlier than
 * the tag that was passed in.
 */
void test_delay_strict_ignores_negative_interval(void) {
  tag_t tag = {.time = 1000, .microstep = 0};

  tag_t result = lf_delay_strict(tag, -5);
  TEST_ASSERT_EQUAL_INT64(1000, result.time);
  TEST_ASSERT_EQUAL_UINT32(0, result.microstep);

  result = lf_delay_strict(tag, NEVER);
  TEST_ASSERT_EQUAL_INT64(1000, result.time);
  TEST_ASSERT_EQUAL_UINT32(0, result.microstep);
}

/** A positive interval must still produce the strictly-earlier tag. */
void test_delay_strict_still_strict_for_positive_interval(void) {
  tag_t tag = {.time = 1000, .microstep = 0};

  tag_t result = lf_delay_strict(tag, 500);
  TEST_ASSERT_EQUAL_INT64(1499, result.time);
  TEST_ASSERT_EQUAL_UINT32(UINT_MAX, result.microstep);
}

/** lf_time_add must saturate rather than wrap on signed overflow. */
void test_time_add_saturates_at_forever(void) {
  TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(FOREVER - 1, 100));
  TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(100, FOREVER - 1));
  TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(FOREVER - 1, 1));
}

/** lf_time_add must saturate rather than wrap on signed underflow. */
void test_time_add_saturates_at_never(void) {
  TEST_ASSERT_EQUAL_INT64(NEVER, lf_time_add(NEVER + 1, -100));
  TEST_ASSERT_EQUAL_INT64(NEVER, lf_time_add(-100, NEVER + 1));
}

/** Ordinary additions must be unaffected by the saturation guards. */
void test_time_add_normal_cases(void) {
  TEST_ASSERT_EQUAL_INT64(1500, lf_time_add(1000, 500));
  TEST_ASSERT_EQUAL_INT64(500, lf_time_add(1000, -500));
  TEST_ASSERT_EQUAL_INT64(NEVER, lf_time_add(NEVER, 500));
  TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(FOREVER, 500));
}

/** lf_tag_add must decide on overflow before performing the addition. */
void test_tag_add_saturates_at_forever(void) {
  tag_t big = {.time = FOREVER - 1, .microstep = 0};
  tag_t delta = {.time = 100, .microstep = 0};

  tag_t result = lf_tag_add(big, delta);
  TEST_ASSERT_EQUAL_INT64(FOREVER, result.time);
}

/** lf_tag_add must decide on underflow before performing the addition. */
void test_tag_add_saturates_at_never(void) {
  tag_t small = {.time = NEVER + 1, .microstep = 0};
  tag_t delta = {.time = -100, .microstep = 0};

  tag_t result = lf_tag_add(small, delta);
  TEST_ASSERT_EQUAL_INT64(NEVER, result.time);
}

/** Ordinary tag additions must be unaffected by the saturation guards. */
void test_tag_add_normal_cases(void) {
  tag_t a = {.time = 1000, .microstep = 3};
  tag_t zero_time = {.time = 0, .microstep = 4};
  tag_t positive_time = {.time = 500, .microstep = 0};

  tag_t result = lf_tag_add(a, zero_time);
  TEST_ASSERT_EQUAL_INT64(1000, result.time);
  TEST_ASSERT_EQUAL_UINT32(7, result.microstep);

  // A second argument with time > 0 drops the microstep of the first.
  result = lf_tag_add(a, positive_time);
  TEST_ASSERT_EQUAL_INT64(1500, result.time);
  TEST_ASSERT_EQUAL_UINT32(0, result.microstep);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_time_unit_macros_parenthesize_argument);
  RUN_TEST(test_delay_strict_ignores_negative_interval);
  RUN_TEST(test_delay_strict_still_strict_for_positive_interval);
  RUN_TEST(test_time_add_saturates_at_forever);
  RUN_TEST(test_time_add_saturates_at_never);
  RUN_TEST(test_time_add_normal_cases);
  RUN_TEST(test_tag_add_saturates_at_forever);
  RUN_TEST(test_tag_add_saturates_at_never);
  RUN_TEST(test_tag_add_normal_cases);
  return UNITY_END();
}
