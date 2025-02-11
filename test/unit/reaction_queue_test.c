#include "reactor-uc/environment.h"
#include "reactor-uc/queues.h"
#include "unity.h"

void test_insert(void) {
  ReactionQueue q;
  Reaction rs[REACTION_QUEUE_SIZE];
  ReactionQueue_ctor(&q);
  TEST_ASSERT_TRUE(q.empty(&q));
  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    rs[i].level = REACTION_QUEUE_SIZE - 1 - i;
    q.insert(&q, &rs[i]);
    TEST_ASSERT_FALSE(q.empty(&q));
  }

  for (int i = REACTION_QUEUE_SIZE - 1; i >= 0; i--) {
    TEST_ASSERT_FALSE(q.empty(&q));
    Reaction *r = q.pop(&q);
    TEST_ASSERT_EQUAL_PTR(r, &rs[i]);
  }
  TEST_ASSERT_TRUE(q.empty(&q));
}

void test_levels_with_gaps(void) {
  ReactionQueue q;
  Reaction rs[REACTION_QUEUE_SIZE];
  ReactionQueue_ctor(&q);
  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    if (i < REACTION_QUEUE_SIZE / 2) {
      rs[i].level = 1;
    } else {
      rs[i].level = i;
    }
    q.insert(&q, &rs[i]);
  }

  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    Reaction *r = q.pop(&q);
    TEST_ASSERT_EQUAL_PTR(r, &rs[i]);
  }
}
Environment *_lf_environment = NULL;

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  RUN_TEST(test_levels_with_gaps);
  return UNITY_END();
}