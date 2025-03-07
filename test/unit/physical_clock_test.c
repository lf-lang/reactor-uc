#include "unity.h"

#include "reactor-uc/environment.h"
#include "reactor-uc/physical_clock.h"

Environment env;
Environment *_lf_environment = &env;

void smoke_test(void) {
  PhysicalClock clock;
  PhysicalClock_ctor(&clock, env.platform, true);

  instant_t t1 = clock.get_time(&clock);
  instant_t t2 = clock.get_time(&clock);
  TEST_ASSERT_TRUE(t1 <= t2);

  clock.set_time(&clock, SEC(100));
  instant_t t3 = clock.get_time(&clock);
  TEST_ASSERT_GREATER_OR_EQUAL(SEC(100), t3);

  clock.adjust_time(&clock, 1000);
  TEST_ASSERT_EQUAL(1000, clock.adjustment_ppb);
  instant_t t4 = clock.get_time(&clock);
  TEST_ASSERT_GREATER_OR_EQUAL(SEC(100), t4);
}

int main(void) {
  Environment_ctor(&env, NULL, NEVER, NULL, NULL, NULL, false, false, false, NULL, 0, NULL, NULL);

  UNITY_BEGIN();
  RUN_TEST(smoke_test);
  return UNITY_END();
}