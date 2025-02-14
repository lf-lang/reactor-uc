#include "reactor-uc/reactor-uc.h"
#include "unity.h"

void test_insert(void) {
  EventQueue q;
  lf_ret_t ret;
  ArbitraryEvent array[10];
  EventQueue_ctor(&q, array, 10);
  TEST_ASSERT_TRUE(q.empty(&q));
  tag_t t = FOREVER_TAG;
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), t), 0);
  Event e = {.super.tag = {.time = 100}};
  ret = q.insert(&q, &e.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);

  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e.super.tag), 0);
  Event e2 = {.super.tag = {.time = 50}};
  ret = q.insert(&q, &e2.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e2.super.tag), 0);

  Event e3 = {.super.tag = {.time = 150}};
  ret = q.insert(&q, &e3.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e2.super.tag), 0);
  Event eptr;
  ret =  q.pop(&q, &eptr.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.super.tag, e2.super.tag), 0);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e.super.tag), 0);

  ret = q.pop(&q, &eptr.super);
  TEST_ASSERT_EQUAL(LF_OK, ret);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.super.tag, e.super.tag), 0);
  TEST_ASSERT_EQUAL(lf_tag_compare(q.next_tag(&q), e3.super.tag), 0);

  ret = q.pop(&q, &eptr.super);
  TEST_ASSERT_EQUAL(lf_tag_compare(eptr.super.tag, e3.super.tag), 0);
}

Environment *_lf_environment = NULL;
int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  return UNITY_END();
}