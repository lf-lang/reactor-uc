#include "reactor-uc/reactor-uc.h"
#include "unity.h"

void test_insert(void) {
  EventQueue q;
  EventQueue_ctor(&q);
  TEST_ASSERT_TRUE(q.empty(&q));
  tag_t t = FOREVER_TAG;
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), t), 0);
  Event e = {.tag = {.time = 100}};
  q.insert(&q, &e);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e.tag), 0);

  Event e2 = {.tag = {.time = 50}};
  q.insert(&q, &e2);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e2.tag), 0);

  Event e3 = {.tag = {.time = 150}};
  q.insert(&q, &e3);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e2.tag), 0);

  Event eptr = q.pop(&q);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.tag, e2.tag), 0);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e.tag), 0);

  eptr = q.pop(&q);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.tag, e.tag), 0);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e3.tag), 0);

  eptr = q.pop(&q);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.tag, e3.tag), 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  return UNITY_END();
}