#include "reactor-uc/environment.h"
#include "reactor-uc/queues.h"
#include "unity.h"
// 65 rather than a round number: the occupancy bitmap then spans more than one
// word for both supported word widths, so the multiword scan is exercised.
#define REACTION_QUEUE_SIZE 65
#define REACTION_QUEUE_WORDS LF_LEVEL_WORDS(REACTION_QUEUE_SIZE)
void test_insert(void) {
  ReactionQueue q;
  lf_level_word_t level_occupied[REACTION_QUEUE_WORDS];
  Reaction* level_tail[REACTION_QUEUE_SIZE];
  Reaction rs[REACTION_QUEUE_SIZE];
  ReactionQueue_ctor(&q, level_tail, level_occupied, REACTION_QUEUE_SIZE);

  for (size_t i = 0; i < REACTION_QUEUE_SIZE; i++) {
    TEST_ASSERT_NULL(level_tail[i]);
  }

  TEST_ASSERT_TRUE(q.empty(&q));
  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    rs[i].level = REACTION_QUEUE_SIZE - 1 - i;
    q.insert(&q, &rs[i]);
    TEST_ASSERT_FALSE(q.empty(&q));
  }

  for (int i = REACTION_QUEUE_SIZE - 1; i >= 0; i--) {
    TEST_ASSERT_FALSE(q.empty(&q));
    Reaction* r = q.pop(&q);
    TEST_ASSERT_EQUAL_PTR(r, &rs[i]);
  }
  TEST_ASSERT_TRUE(q.empty(&q));
}

void test_levels_with_gaps(void) {
  ReactionQueue q;
  lf_level_word_t level_occupied[REACTION_QUEUE_WORDS];
  Reaction* level_tail[REACTION_QUEUE_SIZE];
  Reaction rs[REACTION_QUEUE_SIZE];
  ReactionQueue_ctor(&q, level_tail, level_occupied, REACTION_QUEUE_SIZE);
  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    if (i < REACTION_QUEUE_SIZE / 2) {
      rs[i].level = 1;
    } else {
      rs[i].level = i;
    }
    q.insert(&q, &rs[i]);
  }

  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    Reaction* r = q.pop(&q);
    TEST_ASSERT_EQUAL_PTR(r, &rs[i]);
  }
}
Environment* _lf_environment = NULL;

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  RUN_TEST(test_levels_with_gaps);
  return UNITY_END();
}