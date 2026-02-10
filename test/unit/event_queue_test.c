#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#define QUEUE_SIZE 10
#define N_INSERTS 5
static Trigger trigger_a;
static Trigger trigger_b;

void test_empty(void) {
  // Test that an empty queue returns FOREVER_TAG as the next tag and LF_EVENT_QUEUE_EMPTY when popping.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);
  TEST_ASSERT_TRUE(q.empty(&q));
  TEST_ASSERT_EQUAL(0, q.size);
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), FOREVER_TAG));
  Event e;
  TEST_ASSERT_EQUAL(LF_EVENT_QUEUE_EMPTY, q.pop(&q, &e.super));
}

void test_insert(void) {
  // Test that events are inserted in the correct order and that next_tag returns the correct tag.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);
  lf_ret_t ret;

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
  ret = q.pop(&q, &eptr.super);
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

void test_insert_full(void) {
  // Test that inserting into a full queue returns LF_EVENT_QUEUE_FULL and does not modify the queue.
  EventQueue q;
  ArbitraryEvent array[2];
  EventQueue_ctor(&q, array, 2);

  Event e1 = {.super.tag = {.time = 10}};
  Event e2 = {.super.tag = {.time = 20}};
  Event e3 = {.super.tag = {.time = 30}};
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e1.super));
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e2.super));
  TEST_ASSERT_EQUAL(LF_EVENT_QUEUE_FULL, q.insert(&q, &e3.super));
  TEST_ASSERT_EQUAL(2, q.size);
}

void test_zero_capacity_event_queue(void) {
  // Test that an event queue with zero capacity always returns LF_EVENT_QUEUE_FULL when inserting and
  // LF_EVENT_QUEUE_EMPTY when popping.
  EventQueue q;
  Event e = {.super.tag = {.time = 150}};
  EventQueue_ctor(&q, NULL, 0);
  TEST_ASSERT_TRUE(q.empty(&q));
  TEST_ASSERT_EQUAL(0, q.capacity);
  TEST_ASSERT_EQUAL(0, q.size);
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), FOREVER_TAG));
  TEST_ASSERT_EQUAL(LF_EVENT_QUEUE_FULL, q.insert(&q, &e.super));
  TEST_ASSERT_EQUAL(LF_EVENT_QUEUE_EMPTY, q.pop(&q, &e.super));
}

void test_pop(void) {
  // Test that events are popped in the correct order (first by time, then by microstep).
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  for (size_t i = N_INSERTS; i > 0; i--) {
    for (size_t j = 2; j > 0; j--) {
      Event e = {.super.tag = {.time = (i - 1) * 10, .microstep = j - 1}};
      q.insert(&q, &e.super);
    }
  }

  Event out;
  for (size_t i = 0; i < N_INSERTS; i++) {
    for (size_t j = 0; j < 2; j++) {
      TEST_ASSERT_EQUAL(LF_OK, q.pop(&q, &out.super));
      TEST_ASSERT_EQUAL(i * 10, out.super.tag.time);
      TEST_ASSERT_EQUAL(j, out.super.tag.microstep);
    }
  }

  TEST_ASSERT_TRUE(q.empty(&q));
  TEST_ASSERT_EQUAL(LF_EVENT_QUEUE_EMPTY, q.pop(&q, &out.super));
}

void test_build_heap(void) {
  // Test that build_heap correctly builds a min-heap from an unordered array of events.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE] = {{.event.super.tag = {.time = 50}},
                                      {.event.super.tag = {.time = 10}},
                                      {.event.super.tag = {.time = 40}},
                                      {.event.super.tag = {.time = 30}},
                                      {.event.super.tag = {.time = 20}}};
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  q.size = 5;
  q.build_heap(&q);

  Event out;
  instant_t expected[] = {10, 20, 30, 40, 50};
  for (size_t i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(LF_OK, q.pop(&q, &out.super));
    TEST_ASSERT_EQUAL(expected[i], out.super.tag.time);
  }
  TEST_ASSERT_TRUE(q.empty(&q));
}

void test_heapify(void) {
  // Test that heapify correctly maintains the min-heap property after modifying the tag of an event.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  for (size_t i = 0; i < N_INSERTS; i++) {
    Event e = {.super.tag = {.time = (i + 1) * 10}};
    q.insert(&q, &e.super);
  }

  // Modify the tag of the root event so that it violates the min-heap property
  array[0].event.super.tag.time = 999;
  q.heapify(&q, 0);

  TEST_ASSERT_EQUAL(20, q.next_tag(&q).time);

  Event out;
  for (size_t i = 0; i < N_INSERTS - 1; i++) {
    q.pop(&q, &out.super);
    TEST_ASSERT_EQUAL((i + 2) * 10, out.super.tag.time);
  }
  q.pop(&q, &out.super);
  TEST_ASSERT_EQUAL(999, out.super.tag.time);
}

void test_find_equal_same_tag(void) {
  // Test that find_equal_same_tag finds an event with the same tag and trigger, and returns NULL if no such event
  // exists.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  Event e1 = EVENT_INIT(((tag_t){.time = 100, .microstep = 0}), &trigger_a, NULL);
  Event e2 = EVENT_INIT(((tag_t){.time = 200, .microstep = 0}), &trigger_b, NULL);
  Event e3 = EVENT_INIT(((tag_t){.time = 100, .microstep = 0}), &trigger_b, NULL);
  q.insert(&q, &e1.super);
  q.insert(&q, &e2.super);
  q.insert(&q, &e3.super);

  // Search for an event at tag (100,0) with trigger_a (e1)
  Event search1 = EVENT_INIT(((tag_t){.time = 100, .microstep = 0}), &trigger_a, NULL);
  ArbitraryEvent* found = q.find_equal_same_tag(&q, &search1.super);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_PTR(&trigger_a, found->event.trigger);

  // Search for an event at tag (100,0) with trigger_b (e3)
  Event search2 = EVENT_INIT(((tag_t){.time = 100, .microstep = 0}), &trigger_b, NULL);
  found = q.find_equal_same_tag(&q, &search2.super);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_PTR(&trigger_b, found->event.trigger);

  // Search for an event at tag (200,0) with trigger_a (does not exist)
  Event search3 = EVENT_INIT(((tag_t){.time = 200, .microstep = 0}), &trigger_a, NULL);
  found = q.find_equal_same_tag(&q, &search3.super);
  TEST_ASSERT_NULL(found);

  // Search for a tag not in the queue at all
  Event search4 = EVENT_INIT(((tag_t){.time = 999, .microstep = 0}), &trigger_a, NULL);
  found = q.find_equal_same_tag(&q, &search4.super);
  TEST_ASSERT_NULL(found);
}

void test_remove(void) {
  // Test that remove correctly removes an event with a matching tag and trigger, and does nothing if no such event
  // exists.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  Event e1 = EVENT_INIT(((tag_t){.time = 100}), &trigger_a, NULL);
  Event e2 = EVENT_INIT(((tag_t){.time = 200}), &trigger_b, NULL);
  Event e3 = EVENT_INIT(((tag_t){.time = 300}), &trigger_a, NULL);
  q.insert(&q, &e1.super);
  q.insert(&q, &e2.super);
  q.insert(&q, &e3.super);
  TEST_ASSERT_EQUAL(3, q.size);

  // Remove event e2
  Event t1 = EVENT_INIT(((tag_t){.time = 200}), &trigger_b, NULL);
  TEST_ASSERT_EQUAL(LF_OK, q.remove(&q, &t1.super));
  TEST_ASSERT_EQUAL(2, q.size);

  // Remove non-existent event (should do nothing)
  Event t2 = EVENT_INIT(((tag_t){.time = 999}), &trigger_b, NULL);
  TEST_ASSERT_EQUAL(LF_OK, q.remove(&q, &t2.super));
  TEST_ASSERT_EQUAL(2, q.size);

  // Remove event e1 (root)
  TEST_ASSERT_EQUAL(100, q.next_tag(&q).time);
  Event t3 = EVENT_INIT(((tag_t){.time = 100}), &trigger_a, NULL);
  TEST_ASSERT_EQUAL(LF_OK, q.remove(&q, &t3.super));
  TEST_ASSERT_EQUAL(1, q.size);

  // Verify remaining events pop in order
  Event out;
  q.pop(&q, &out.super);
  TEST_ASSERT_EQUAL(300, out.super.tag.time);
  TEST_ASSERT_TRUE(q.empty(&q));
}

Environment* _lf_environment = NULL;
int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_insert);
  RUN_TEST(test_zero_capacity_event_queue);
  RUN_TEST(test_build_heap);
  RUN_TEST(test_heapify);
  RUN_TEST(test_find_equal_same_tag);
  RUN_TEST(test_remove);
  return UNITY_END();
}
