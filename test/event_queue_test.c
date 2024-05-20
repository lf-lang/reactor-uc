#include "unity.h"

#define EVENT_QUEUE_SIZE 5
#include "reactor-uc/queues.h"

void test_insert(void) {
  EventQueue q;
  EventQueue_ctor(&q);
  TEST_ASSERT_TRUE(q.empty(&q));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  return UNITY_END();
}