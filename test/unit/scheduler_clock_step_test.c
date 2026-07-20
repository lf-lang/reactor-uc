#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

#define QUEUE_CAPACITY 4

typedef struct {
  DynamicScheduler scheduler;
  EventQueue event_queue;
  ArbitraryEvent event_storage[QUEUE_CAPACITY];
  EventQueue system_event_queue;
  ArbitraryEvent system_event_storage[QUEUE_CAPACITY];
  ReactionQueue reaction_queue;
  Reaction* reaction_storage[1];
  int reaction_level_size[1];
} SchedulerFixture;

static void fixture_ctor(SchedulerFixture* fixture) {
  EventQueue_ctor(&fixture->event_queue, fixture->event_storage, QUEUE_CAPACITY);
  EventQueue_ctor(&fixture->system_event_queue, fixture->system_event_storage, QUEUE_CAPACITY);
  ReactionQueue_ctor(&fixture->reaction_queue, fixture->reaction_storage, fixture->reaction_level_size, 1);
  DynamicScheduler_ctor(&fixture->scheduler, NULL, &fixture->event_queue, &fixture->system_event_queue,
                        &fixture->reaction_queue, FOREVER, false);
}

static void insert_system_event(EventQueue* queue, instant_t time) {
  SystemEvent event = SYSTEM_EVENT_INIT(((tag_t){.time = time}), NULL, NULL);
  TEST_ASSERT_EQUAL(LF_OK, queue->insert(queue, &event.super));
}

static instant_t pop_system_event_time(EventQueue* queue) {
  SystemEvent event;
  TEST_ASSERT_EQUAL(LF_OK, queue->pop(queue, &event.super));
  return event.super.tag.time;
}

void test_step_clock_shifts_system_events_in_place(void) {
  SchedulerFixture fixture;
  fixture_ctor(&fixture);
  insert_system_event(&fixture.system_event_queue, SEC(1));
  insert_system_event(&fixture.system_event_queue, SEC(2));

  fixture.scheduler.super.step_clock(&fixture.scheduler.super, SEC(10));

  TEST_ASSERT_EQUAL_INT64(SEC(11), pop_system_event_time(&fixture.system_event_queue));
  TEST_ASSERT_EQUAL_INT64(SEC(12), pop_system_event_time(&fixture.system_event_queue));
}

void test_step_clock_clamps_negative_system_event_times_to_zero(void) {
  SchedulerFixture fixture;
  fixture_ctor(&fixture);
  insert_system_event(&fixture.system_event_queue, SEC(1));
  insert_system_event(&fixture.system_event_queue, SEC(12));

  fixture.scheduler.super.step_clock(&fixture.scheduler.super, -SEC(10));

  TEST_ASSERT_EQUAL_INT64(0, pop_system_event_time(&fixture.system_event_queue));
  TEST_ASSERT_EQUAL_INT64(SEC(2), pop_system_event_time(&fixture.system_event_queue));
}

void test_step_clock_saturates_system_event_time_overflow(void) {
  SchedulerFixture fixture;
  fixture_ctor(&fixture);
  insert_system_event(&fixture.system_event_queue, FOREVER - 5);

  fixture.scheduler.super.step_clock(&fixture.scheduler.super, 10);

  TEST_ASSERT_EQUAL_INT64(FOREVER, pop_system_event_time(&fixture.system_event_queue));
}

Environment* _lf_environment = NULL;

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_step_clock_shifts_system_events_in_place);
  RUN_TEST(test_step_clock_clamps_negative_system_event_times_to_zero);
  RUN_TEST(test_step_clock_saturates_system_event_time_overflow);
  return UNITY_END();
}
