#include "unity.h"

#include "reactor-uc/environment.h"
#include "reactor-uc/physical_clock.h"

Environment env;
Environment *_lf_environment = &env;

static instant_t hw_time = 0;
instant_t mock_get_physical_time(Platform *self) {
  (void)self;
  return hw_time;
}

Platform p = {.get_physical_time = mock_get_physical_time};

void smoke_test(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &p, true);

  instant_t t1 = clock.get_time(&clock);
  instant_t t2 = clock.get_time(&clock);
  TEST_ASSERT_TRUE(t1 <= t2);

  clock.set_time(&clock, SEC(100));
  instant_t t3 = clock.get_time(&clock);
  TEST_ASSERT_GREATER_OR_EQUAL(SEC(100), t3);

  clock.adjust_time(&clock, 1000);
  instant_t t4 = clock.get_time(&clock);
  TEST_ASSERT_GREATER_OR_EQUAL(SEC(100), t4);
}

void test_to_hw_time_no_adj(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &p, true);

  clock.offset = SEC(170000);
  clock.adjustment_epoch_hw = MSEC(100);
  clock.adjustment = 0.0;

  instant_t time = SEC(170001);
  instant_t t1 = clock.to_hw_time(&clock, time);
  TEST_ASSERT_EQUAL(time - clock.offset, t1);

  time = DAYS(1000);
  t1 = clock.to_hw_time(&clock, time);
  TEST_ASSERT_EQUAL(time - clock.offset, t1);

  time = SEC(1741708351);
  t1 = clock.to_hw_time(&clock, time);
  TEST_ASSERT_EQUAL(time - clock.offset, t1);

  t1 = clock.to_hw_time(&clock, FOREVER);
  TEST_ASSERT_EQUAL(FOREVER, t1);

  t1 = clock.to_hw_time(&clock, NEVER);
  TEST_ASSERT_EQUAL(NEVER, t1);
}

void test_to_hw_time_with_adj(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &p, true);
  instant_t time, hw_time;

  // Test that 1ppb for 1 second means 1 nsec added
  clock.offset = 0;
  clock.adjustment_epoch_hw = 0;
  clock.adjustment = -1.0 / BILLION;
  time = SEC(1);
  hw_time = clock.to_hw_time(&clock, time);
  TEST_ASSERT_EQUAL(SEC(1) + NSEC(1), hw_time);

  // Test that 1ppb for 500sec => 500 nsec
  clock.offset = SEC(1000);
  clock.adjustment_epoch_hw = SEC(500);
  clock.adjustment = -1.0 / BILLION;
  time = SEC(2000);
  hw_time = clock.to_hw_time(&clock, time);
  TEST_ASSERT_EQUAL(SEC(1000) + NSEC(500), hw_time);

  // Test adjustment the other way
  clock.offset = 0;
  clock.adjustment_epoch_hw = 0;
  clock.adjustment = 100000.0 / BILLION; // 100k ppb

  time = SEC(10);
  hw_time = clock.to_hw_time(&clock, time);
  TEST_ASSERT_EQUAL(9999000099, hw_time);
}

void test_get_set_time(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &p, true);
  instant_t t;
  lf_ret_t ret;
  hw_time = 0;

  clock.offset = 0;
  clock.adjustment = 0.0;
  clock.adjustment_epoch_hw = 0;
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(0, t);

  clock.offset = 1234;
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(1234, t);

  hw_time = 2000;
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(hw_time + 1234, t);

  ret = clock.set_time(&clock, 3000);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  hw_time = 3000;
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(4000, t);

  ret = clock.set_time(&clock, 0);
  TEST_ASSERT_EQUAL(LF_OK, ret);

  ret = clock.set_time(&clock, -1);
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, ret);
}

void test_adjust_time(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &p, true);
  instant_t t;
  lf_ret_t ret;
  hw_time = 0;

  clock.adjust_time(&clock, 1000);
  hw_time = SEC(1);
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(SEC(1) + USEC(1), t);

  clock.adjust_time(&clock, 2000);
  hw_time = SEC(2);
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(SEC(2) + USEC(1) + USEC(2), t);

  clock.adjust_time(&clock, -3000);
  hw_time = SEC(3);
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(SEC(3), t);

  clock.adjust_time(&clock, 1000000000);
  hw_time = SEC(4);
  t = clock.get_time(&clock);
  TEST_ASSERT_EQUAL(SEC(4) + SEC(1), t);
}

int main(void) {
  Environment_ctor(&env, NULL, NEVER, NULL, NULL, NULL, false, false, false, NULL, 0, NULL, NULL);

  UNITY_BEGIN();
  RUN_TEST(smoke_test);
  RUN_TEST(test_to_hw_time_no_adj);
  RUN_TEST(test_to_hw_time_with_adj);
  RUN_TEST(test_get_set_time);
  RUN_TEST(test_adjust_time);
  return UNITY_END();
}
