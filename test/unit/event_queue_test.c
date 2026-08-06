#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#include <stdio.h>

#define QUEUE_SIZE 10
#define N_INSERTS 5

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static Trigger trigger_a;
static Trigger trigger_b;
static SystemEventHandler handler_a;
static SystemEventHandler handler_b;

/**
 * @brief Assert that every parent tag is <= both of its children.
 * Use through ASSERT_HEAP_INVARIANT so failures point at the caller.
 */
static void assert_heap_invariant(EventQueue* q, UNITY_LINE_TYPE line) {
  for (size_t i = 1; i < q->size; i++) {
    size_t parent = (i - 1) / 2;
    tag_t parent_tag = get_tag(&q->array[parent]);
    tag_t child_tag = get_tag(&q->array[i]);
    if (lf_tag_compare(parent_tag, child_tag) <= 0) {
      continue;
    }
    char msg[128];
    snprintf(msg, sizeof(msg), "min-heap invariant violated: array[%zu]=" PRINTF_TAG " > array[%zu]=" PRINTF_TAG,
             parent, parent_tag.time, parent_tag.microstep, i, child_tag.time, child_tag.microstep);
    UNITY_TEST_FAIL(line, msg);
  }
}
#define ASSERT_HEAP_INVARIANT(q) assert_heap_invariant((q), __LINE__)

/**
 * @brief Drain the queue and assert the events come out with exactly @p expected times, and that the queue is empty afterwards.
 * Use through ASSERT_POPS_IN_ORDER so failures point at the caller.
 */
static void assert_pops_in_order(EventQueue* q, const instant_t* expected, size_t n, UNITY_LINE_TYPE line) {
  // An ArbitraryEvent is the only buffer guaranteed to fit whatever pop() copies out.
  ArbitraryEvent out;
  char msg[32];
  for (size_t i = 0; i < n; i++) {
    snprintf(msg, sizeof(msg), "pop #%zu", i);
    UNITY_TEST_ASSERT_EQUAL_INT(LF_OK, q->pop(q, &out.event.super), line, msg);
    UNITY_TEST_ASSERT_EQUAL_INT64(expected[i], get_tag(&out).time, line, msg);
  }
  UNITY_TEST_ASSERT(q->empty(q), line, "queue is not empty after popping every expected event");
}
#define ASSERT_POPS_IN_ORDER(q, expected) assert_pops_in_order((q), (expected), ARRAY_SIZE(expected), __LINE__)

static void insert_at_time(EventQueue* q, instant_t t) {
  Event e = EVENT_INIT(((tag_t){.time = t}), &trigger_a, NULL);
  TEST_ASSERT_EQUAL(LF_OK, q->insert(q, &e.super));
}

static lf_ret_t remove_at_time(EventQueue* q, instant_t t) {
  Event e = EVENT_INIT(((tag_t){.time = t}), &trigger_a, NULL);
  return q->remove(q, &e.super);
}

static bool contains_time(EventQueue* q, instant_t t) {
  Event e = EVENT_INIT(((tag_t){.time = t}), &trigger_a, NULL);
  return q->find_equal_same_tag(q, &e.super) != NULL;
}

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

  TEST_ASSERT_TRUE(q.empty(&q));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), FOREVER_TAG));

  Event e = {.super.tag = {.time = 100}};
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e.super));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), e.super.tag));

  Event e2 = {.super.tag = {.time = 50}};
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e2.super));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), e2.super.tag));

  Event e3 = {.super.tag = {.time = 150}};
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e3.super));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), e2.super.tag));
  ASSERT_HEAP_INVARIANT(&q);

  Event out;
  TEST_ASSERT_EQUAL(LF_OK, q.pop(&q, &out.super));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(out.super.tag, e2.super.tag));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), e.super.tag));

  TEST_ASSERT_EQUAL(LF_OK, q.pop(&q, &out.super));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(out.super.tag, e.super.tag));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), e3.super.tag));

  TEST_ASSERT_EQUAL(LF_OK, q.pop(&q, &out.super));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(out.super.tag, e3.super.tag));
  TEST_ASSERT_TRUE(q.empty(&q));
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

  // The rejected event must not have displaced anything already queued.
  const instant_t expect[] = {10, 20};
  ASSERT_POPS_IN_ORDER(&q, expect);
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
      Event e = {.super.tag = {.time = (instant_t)(i - 1) * 10, .microstep = (microstep_t)(j - 1)}};
      TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e.super));
    }
  }
  ASSERT_HEAP_INVARIANT(&q);

  Event out;
  for (size_t i = 0; i < N_INSERTS; i++) {
    for (size_t j = 0; j < 2; j++) {
      TEST_ASSERT_EQUAL(LF_OK, q.pop(&q, &out.super));
      TEST_ASSERT_EQUAL_INT64((instant_t)i * 10, out.super.tag.time);
      TEST_ASSERT_EQUAL(j, out.super.tag.microstep);
    }
  }

  TEST_ASSERT_TRUE(q.empty(&q));
  TEST_ASSERT_EQUAL(LF_EVENT_QUEUE_EMPTY, q.pop(&q, &out.super));
}

void test_build_heap(void) {
  // Test that build_heap correctly builds a min-heap from an unordered array of events.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  const instant_t unordered[] = {50, 10, 40, 30, 20};
  for (size_t i = 0; i < ARRAY_SIZE(unordered); i++) {
    array[i].event = (Event)EVENT_INIT(((tag_t){.time = unordered[i]}), &trigger_a, NULL);
  }
  q.size = ARRAY_SIZE(unordered);
  q.build_heap(&q);
  ASSERT_HEAP_INVARIANT(&q);

  const instant_t expect[] = {10, 20, 30, 40, 50};
  ASSERT_POPS_IN_ORDER(&q, expect);
}

void test_heapify(void) {
  // Test that heapify correctly maintains the min-heap property after modifying the tag of an event.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  for (size_t i = 0; i < N_INSERTS; i++) {
    insert_at_time(&q, (instant_t)(i + 1) * 10);
  }

  // Modify the tag of the root event so that it violates the min-heap property
  array[0].event.super.tag.time = 999;
  q.heapify(&q, 0);
  ASSERT_HEAP_INVARIANT(&q);

  TEST_ASSERT_EQUAL_INT64(20, q.next_tag(&q).time);

  const instant_t expect[] = {20, 30, 40, 50, 999};
  ASSERT_POPS_IN_ORDER(&q, expect);
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
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e1.super));
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e2.super));
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e3.super));

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

  // A matching trigger at a different microstep is not a match.
  Event search3 = EVENT_INIT(((tag_t){.time = 100, .microstep = 1}), &trigger_a, NULL);
  TEST_ASSERT_NULL(q.find_equal_same_tag(&q, &search3.super));

  // Search for an event at tag (200,0) with trigger_a (does not exist)
  Event search4 = EVENT_INIT(((tag_t){.time = 200, .microstep = 0}), &trigger_a, NULL);
  TEST_ASSERT_NULL(q.find_equal_same_tag(&q, &search4.super));

  // Search for a tag not in the queue at all
  Event search5 = EVENT_INIT(((tag_t){.time = 999, .microstep = 0}), &trigger_a, NULL);
  TEST_ASSERT_NULL(q.find_equal_same_tag(&q, &search5.super));
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
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e1.super));
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e2.super));
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e3.super));
  TEST_ASSERT_EQUAL(3, q.size);

  // Remove event e2
  Event t1 = EVENT_INIT(((tag_t){.time = 200}), &trigger_b, NULL);
  TEST_ASSERT_EQUAL(LF_OK, q.remove(&q, &t1.super));
  TEST_ASSERT_EQUAL(2, q.size);
  TEST_ASSERT_NULL(q.find_equal_same_tag(&q, &t1.super));
  ASSERT_HEAP_INVARIANT(&q);

  // Removing it a second time reports that it is gone.
  TEST_ASSERT_EQUAL(LF_EVENT_NOT_FOUND, q.remove(&q, &t1.super));
  TEST_ASSERT_EQUAL(2, q.size);

  // Remove non-existent event (should do nothing)
  Event t2 = EVENT_INIT(((tag_t){.time = 999}), &trigger_b, NULL);
  TEST_ASSERT_EQUAL(LF_EVENT_NOT_FOUND, q.remove(&q, &t2.super));
  TEST_ASSERT_EQUAL(2, q.size);

  // A matching tag with a non-matching trigger is not removed either.
  Event t3 = EVENT_INIT(((tag_t){.time = 300}), &trigger_b, NULL);
  TEST_ASSERT_EQUAL(LF_EVENT_NOT_FOUND, q.remove(&q, &t3.super));
  TEST_ASSERT_EQUAL(2, q.size);

  // Remove event e1 (root)
  TEST_ASSERT_EQUAL_INT64(100, q.next_tag(&q).time);
  Event t4 = EVENT_INIT(((tag_t){.time = 100}), &trigger_a, NULL);
  TEST_ASSERT_EQUAL(LF_OK, q.remove(&q, &t4.super));
  TEST_ASSERT_EQUAL(1, q.size);

  // Verify remaining events pop in order
  const instant_t expect[] = {300};
  ASSERT_POPS_IN_ORDER(&q, expect);
}

void test_remove_from_empty_queue(void) {
  // Removing from an empty queue must report LF_EVENT_NOT_FOUND without touching size.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  TEST_ASSERT_EQUAL(LF_EVENT_NOT_FOUND, remove_at_time(&q, 10));
  TEST_ASSERT_EQUAL(0, q.size);
  TEST_ASSERT_TRUE(q.empty(&q));
}

void test_remove_requires_sift_up(void) {
  /* This test populates the heap with the following layout:
       idx:  0   1   2   3   4   5   6
       tag:  1   9   2  10  11   3   4
     Every insert lands at the end and stops immediately (9>=1, 2>=1, 10>=9,
     11>=9, 3>=2, 4>=2). Removing idx3 swaps in the last element (4), whose new
     parent is idx1 (9). heapify() only walks DOWN and idx3 has no children after
     the shrink, so without a sift-up the pair (9, 4) is left inverted. */
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  const instant_t layout[] = {1, 9, 2, 10, 11, 3, 4};
  for (size_t i = 0; i < ARRAY_SIZE(layout); i++) {
    insert_at_time(&q, layout[i]);
  }

  // Guard: the sift-up path is only exercised if insert() produced exactly the layout above.
  for (size_t i = 0; i < ARRAY_SIZE(layout); i++) {
    TEST_ASSERT_EQUAL_INT64(layout[i], get_tag(&q.array[i]).time);
  }
  ASSERT_HEAP_INVARIANT(&q);

  TEST_ASSERT_EQUAL(LF_OK, remove_at_time(&q, 10));
  TEST_ASSERT_EQUAL(ARRAY_SIZE(layout) - 1, q.size);
  TEST_ASSERT_FALSE(contains_time(&q, 10));
  ASSERT_HEAP_INVARIANT(&q);

  const instant_t expect[] = {1, 2, 3, 4, 9, 11};
  ASSERT_POPS_IN_ORDER(&q, expect);
}

void test_remove_requires_sift_down(void) {
  /* Ascending inserts never bubble up, so the heap array is exactly:
       idx:  0  1  2  3  4  5  6
       tag:  1  2  3  4  5  6  7
     Removing idx1 (tag 2) swaps in the last element (7), which is larger than
     both of its new children (4 and 5) and must therefore sift DOWN. This is the
     mirror image of test_remove_requires_sift_up. */
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  const instant_t layout[] = {1, 2, 3, 4, 5, 6, 7};
  for (size_t i = 0; i < ARRAY_SIZE(layout); i++) {
    insert_at_time(&q, layout[i]);
  }

  // Guard: the sift-down path is only exercised if insert() produced exactly the layout above.
  for (size_t i = 0; i < ARRAY_SIZE(layout); i++) {
    TEST_ASSERT_EQUAL_INT64(layout[i], get_tag(&q.array[i]).time);
  }

  TEST_ASSERT_EQUAL(LF_OK, remove_at_time(&q, 2));
  TEST_ASSERT_EQUAL(ARRAY_SIZE(layout) - 1, q.size);
  TEST_ASSERT_FALSE(contains_time(&q, 2));
  ASSERT_HEAP_INVARIANT(&q);

  const instant_t expect[] = {1, 3, 4, 5, 6, 7};
  ASSERT_POPS_IN_ORDER(&q, expect);
}

void test_remove_last_element(void) {
  /* Removing the event that sits in the final array slot leaves nothing to
     re-heapify: after the size decrement the vacated index equals the new size,
     so remove() must skip both heapify() and sift_up() rather than reorder the
     heap around a slot that is no longer live. */
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  insert_at_time(&q, 10);
  insert_at_time(&q, 20);
  insert_at_time(&q, 30);
  // Guard: 30 must really be the last slot for this test to cover that branch.
  TEST_ASSERT_EQUAL_INT64(30, get_tag(&q.array[q.size - 1]).time);

  TEST_ASSERT_EQUAL(LF_OK, remove_at_time(&q, 30));
  TEST_ASSERT_EQUAL(2, q.size);
  TEST_ASSERT_FALSE(contains_time(&q, 30));
  ASSERT_HEAP_INVARIANT(&q);

  const instant_t expect[] = {10, 20};
  ASSERT_POPS_IN_ORDER(&q, expect);
}

void test_remove_sole_element(void) {
  // Removing the only event empties the queue, and the queue stays usable afterwards.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  insert_at_time(&q, 42);
  TEST_ASSERT_EQUAL(LF_OK, remove_at_time(&q, 42));
  TEST_ASSERT_EQUAL(0, q.size);
  TEST_ASSERT_TRUE(q.empty(&q));
  TEST_ASSERT_FALSE(contains_time(&q, 42));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(q.next_tag(&q), FOREVER_TAG));

  insert_at_time(&q, 7);
  TEST_ASSERT_EQUAL_INT64(7, q.next_tag(&q).time);
}

void test_remove_root(void) {
  // Removing the root promotes the next-earliest event.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  insert_at_time(&q, 30);
  insert_at_time(&q, 10);
  insert_at_time(&q, 20);
  TEST_ASSERT_EQUAL_INT64(10, q.next_tag(&q).time);

  TEST_ASSERT_EQUAL(LF_OK, remove_at_time(&q, 10));
  TEST_ASSERT_EQUAL(2, q.size);
  TEST_ASSERT_FALSE(contains_time(&q, 10));
  ASSERT_HEAP_INVARIANT(&q);
  TEST_ASSERT_EQUAL_INT64(20, q.next_tag(&q).time);

  const instant_t expect[] = {20, 30};
  ASSERT_POPS_IN_ORDER(&q, expect);
}

void test_system_event_find_and_remove(void) {
  // System events are matched on their handler rather than on a trigger, and never match a regular
  // event even at an identical tag.
  EventQueue q;
  ArbitraryEvent array[QUEUE_SIZE];
  EventQueue_ctor(&q, array, QUEUE_SIZE);

  SystemEvent s1 = SYSTEM_EVENT_INIT(((tag_t){.time = 100}), &handler_a, NULL);
  SystemEvent s2 = SYSTEM_EVENT_INIT(((tag_t){.time = 100}), &handler_b, NULL);
  Event e1 = EVENT_INIT(((tag_t){.time = 100}), &trigger_a, NULL);
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &s1.super));
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &s2.super));
  TEST_ASSERT_EQUAL(LF_OK, q.insert(&q, &e1.super));
  TEST_ASSERT_EQUAL(3, q.size);

  // A regular event with an unrelated trigger matches neither system event.
  Event search_event = EVENT_INIT(((tag_t){.time = 100}), &trigger_b, NULL);
  TEST_ASSERT_NULL(q.find_equal_same_tag(&q, &search_event.super));

  SystemEvent search_s1 = SYSTEM_EVENT_INIT(((tag_t){.time = 100}), &handler_a, NULL);
  ArbitraryEvent* found = q.find_equal_same_tag(&q, &search_s1.super);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_PTR(&handler_a, found->system_event.handler);

  // Removing s1 leaves s2 and the regular event untouched.
  TEST_ASSERT_EQUAL(LF_OK, q.remove(&q, &search_s1.super));
  TEST_ASSERT_EQUAL(2, q.size);
  TEST_ASSERT_EQUAL(LF_EVENT_NOT_FOUND, q.remove(&q, &search_s1.super));
  ASSERT_HEAP_INVARIANT(&q);

  SystemEvent search_s2 = SYSTEM_EVENT_INIT(((tag_t){.time = 100}), &handler_b, NULL);
  found = q.find_equal_same_tag(&q, &search_s2.super);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_PTR(&handler_b, found->system_event.handler);
  TEST_ASSERT_TRUE(contains_time(&q, 100));

  // s2 and e1 share a tag, so their relative pop order is unspecified: only check the tag.
  const instant_t expect[] = {100, 100};
  ASSERT_POPS_IN_ORDER(&q, expect);
}

Environment* _lf_environment = NULL;

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_empty);
  RUN_TEST(test_insert);
  RUN_TEST(test_insert_full);
  RUN_TEST(test_zero_capacity_event_queue);
  RUN_TEST(test_pop);
  RUN_TEST(test_build_heap);
  RUN_TEST(test_heapify);
  RUN_TEST(test_find_equal_same_tag);
  RUN_TEST(test_remove);
  RUN_TEST(test_remove_from_empty_queue);
  RUN_TEST(test_remove_requires_sift_up);
  RUN_TEST(test_remove_requires_sift_down);
  RUN_TEST(test_remove_last_element);
  RUN_TEST(test_remove_sole_element);
  RUN_TEST(test_remove_root);
  RUN_TEST(test_system_event_find_and_remove);
  return UNITY_END();
}
