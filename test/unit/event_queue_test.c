#include "reactor-uc/reactor-uc.h"
#include "unity.h"

void test_insert(void) {
  EventQueue q;
  lf_ret_t ret;
  ArbitraryEvent array[10];
  EventQueue_ctor(&q, array, 10);
  TEST_ASSERT_TRUE(q.empty_locked(&q));
  tag_t t = FOREVER_TAG;
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag_locked(&q), t), 0);
  Event e = {.super.tag = {.time = 100}};
  ret = q.insert_locked(&q, &e.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);

  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag_locked(&q), e.super.tag), 0);
  Event e2 = {.super.tag = {.time = 50}};
  ret = q.insert_locked(&q, &e2.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag_locked(&q), e2.super.tag), 0);

  Event e3 = {.super.tag = {.time = 150}};
  ret = q.insert_locked(&q, &e3.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag_locked(&q), e2.super.tag), 0);
  Event eptr;
  ret = q.pop_locked(&q, &eptr.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.super.tag, e2.super.tag), 0);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag_locked(&q), e.super.tag), 0);

  ret = q.pop_locked(&q, &eptr.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.super.tag, e.super.tag), 0);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag_locked(&q), e3.super.tag), 0);

  ret = q.pop_locked(&q, &eptr.super);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.super.tag, e3.super.tag), 0);
}

void test_zero_capacity_event_queue(void) {
  EventQueue q;
  Event e = {.super.tag = {.time = 150}};
  EventQueue_ctor(&q, NULL, 0);
  TEST_ASSERT_TRUE(q.empty_locked(&q));
  TEST_ASSERT_EQUAL(0, q.capacity);
  TEST_ASSERT_EQUAL(0, q.size);
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag_locked(&q), FOREVER_TAG));
  TEST_ASSERT_EQUAL(LF_OUT_OF_BOUNDS, q.insert_locked(&q, &e.super));
  TEST_ASSERT_EQUAL(LF_EMPTY, q.pop_locked(&q, &e.super));
}

Environment *_lf_environment = NULL;
int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  RUN_TEST(test_zero_capacity_event_queue);
  return UNITY_END();
}