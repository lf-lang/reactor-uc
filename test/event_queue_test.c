#include "unity.h"

#define EVENT_QUEUE_SIZE 5
#include "reactor-uc/queues.h"

void test_insert(void) {
  EventQueue q;
  EventQueue_ctor(&q);
  TEST_ASSERT_TRUE(q.empty(&q));
  TEST_ASSERT_TRUE(lf_tag_compare(q.next_tag(&q), FOREVER_TAG));
  Event e = {.tag = {.time = 100}};
  q.insert(&q, e);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e.tag), 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  return UNITY_END();
}