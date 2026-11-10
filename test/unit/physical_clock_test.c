#include "unity.h"

#include "reactor-uc/environment.h"
#include "reactor-uc/physical_clock.h"

Environment env;
Environment* _lf_environment = &env;

static instant_t hw_time = 0;
instant_t mock_get_physical_time(Platform* self) {
  (void)self;
  return hw_time;
}

Platform p = {.get_physical_time = mock_get_physical_time};

void smoke_test(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &env, true);

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
  PhysicalClock_ctor(&clock, &env, true);

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
  PhysicalClock_ctor(&clock, &env, true);
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
  PhysicalClock_ctor(&clock, &env, true);
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
  PhysicalClock_ctor(&clock, &env, true);
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

/** step_time must shift the clock by exactly the requested delta, in one locked operation. */
void test_step_time_shifts_by_exact_delta(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &env, true);
  hw_time = SEC(10);

  instant_t before = clock.get_time(&clock);
  lf_ret_t ret = clock.step_time(&clock, SEC(5));
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL_INT64(before + SEC(5), clock.get_time(&clock));

  ret = clock.step_time(&clock, -SEC(2));
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL_INT64(before + SEC(3), clock.get_time(&clock));
}

/** A step that would drive the clock negative must be rejected and change nothing. */
void test_step_time_rejects_negative_result(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &env, true);
  hw_time = SEC(1);

  instant_t before = clock.get_time(&clock);
  lf_ret_t ret = clock.step_time(&clock, -SEC(5));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, ret);
  TEST_ASSERT_EQUAL_INT64(before, clock.get_time(&clock));
}

/**
 * to_hw_time must be exact at POSIX-epoch magnitudes (~1.75e18 ns), where one ulp
 * of a double is 256 ns.
 */
void test_to_hw_time_is_exact_at_epoch_magnitudes(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &env, true);

  clock.offset = 0;
  clock.adjustment = 0.0;
  clock.adjustment_epoch_hw = 0;

  // With no correction applied, to_hw_time is the identity.
  TEST_ASSERT_EQUAL_INT64(1754524800000000128LL, clock.to_hw_time(&clock, 1754524800000000128LL));
  TEST_ASSERT_EQUAL_INT64(1754524800000000384LL, clock.to_hw_time(&clock, 1754524800000000384LL));

  // Distinct instants must not collapse onto the same hardware deadline.
  instant_t a = clock.to_hw_time(&clock, 1754524800000000100LL);
  instant_t b = clock.to_hw_time(&clock, 1754524800000000200LL);
  TEST_ASSERT_TRUE(a != b);
}

/** get_time and to_hw_time must round-trip to within truncation error, not 256 ns. */
void test_to_hw_time_round_trips_with_adjustment(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, &env, true);

  clock.offset = -4321;
  clock.adjustment = 1e-4;
  clock.adjustment_epoch_hw = 1754524800000000000LL;
  hw_time = 1754524800123456789LL;

  instant_t sync = clock.get_time(&clock);
  instant_t back = clock.to_hw_time(&clock, sync);
  TEST_ASSERT_INT64_WITHIN(2, hw_time, back);
}

int main(void) {
  Environment_ctor(&env, NULL, NULL, false);
  env.platform = &p;

  UNITY_BEGIN();
  RUN_TEST(smoke_test);
  RUN_TEST(test_to_hw_time_no_adj);
  RUN_TEST(test_to_hw_time_with_adj);
  RUN_TEST(test_get_set_time);
  RUN_TEST(test_adjust_time);
  RUN_TEST(test_step_time_shifts_by_exact_delta);
  RUN_TEST(test_step_time_rejects_negative_result);
  RUN_TEST(test_to_hw_time_is_exact_at_epoch_magnitudes);
  RUN_TEST(test_to_hw_time_round_trips_with_adjustment);
  return UNITY_END();
}
