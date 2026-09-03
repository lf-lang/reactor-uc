#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

#define QUEUE_SIZE 16

static Environment env;
Environment* _lf_environment = &env; // NOLINT

static instant_t mock_get_physical_time(Platform* self) {
  (void)self;
  return 0;
}
static Platform mock_platform = {.get_physical_time = mock_get_physical_time};

static SystemEventHandler handler;

static DynamicScheduler sched;
static EventQueue event_queue;
static EventQueue system_event_queue;
static ReactionQueue reaction_queue;
static ArbitraryEvent event_queue_array[QUEUE_SIZE];
static ArbitraryEvent system_event_queue_array[QUEUE_SIZE];
// The reaction queue is indexed as [level][index], so it needs capacity^2 slots.
static Reaction* reaction_queue_array[QUEUE_SIZE * QUEUE_SIZE];
static int reaction_queue_level_size[QUEUE_SIZE];

static void setup_scheduler(void) {
  EventQueue_ctor(&event_queue, event_queue_array, QUEUE_SIZE);
  EventQueue_ctor(&system_event_queue, system_event_queue_array, QUEUE_SIZE);
  ReactionQueue_ctor(&reaction_queue, reaction_queue_array, reaction_queue_level_size, QUEUE_SIZE);
  DynamicScheduler_ctor(&sched, &env, &event_queue, &system_event_queue, &reaction_queue, FOREVER, false);
}

static void insert_system_event(instant_t time, microstep_t microstep) {
  tag_t tag = {.time = time, .microstep = microstep};
  SystemEvent event = SYSTEM_EVENT_INIT(tag, &handler, NULL);
  lf_ret_t ret = system_event_queue.insert(&system_event_queue, (AbstractEvent*)&event);
  TEST_ASSERT_EQUAL(LF_OK, ret);
}

/** Drain the system event queue and assert the tags come out in exactly this order. */
static void assert_pops_in_order(const tag_t* expected, size_t n) {
  for (size_t i = 0; i < n; i++) {
    ArbitraryEvent event;
    lf_ret_t ret = system_event_queue.pop(&system_event_queue, (AbstractEvent*)&event);
    TEST_ASSERT_EQUAL(LF_OK, ret);
    TEST_ASSERT_EQUAL_INT64(expected[i].time, event.system_event.super.tag.time);
    TEST_ASSERT_EQUAL_UINT32(expected[i].microstep, event.system_event.super.tag.microstep);
  }
  TEST_ASSERT_TRUE(system_event_queue.empty(&system_event_queue));
}

/** Stepping the clock forward must re-base every pending system event. */
void test_step_clock_shifts_pending_events_forward(void) {
  setup_scheduler();
  insert_system_event(1000, 0);
  insert_system_event(3000, 0);
  insert_system_event(2000, 0);

  sched.super.step_clock(&sched.super, SEC(5));

  tag_t expected[] = {{SEC(5) + 1000, 0}, {SEC(5) + 2000, 0}, {SEC(5) + 3000, 0}};
  assert_pops_in_order(expected, 3);
}

/** Stepping the clock backwards must re-base every pending system event. */
void test_step_clock_shifts_pending_events_backward(void) {
  setup_scheduler();
  insert_system_event(SEC(10), 0);
  insert_system_event(SEC(12), 0);

  sched.super.step_clock(&sched.super, -SEC(5));

  tag_t expected[] = {{SEC(5), 0}, {SEC(7), 0}};
  assert_pops_in_order(expected, 2);
}

/**
 * The clamp at zero is not injective, so a uniform backwards step can invert the
 * relative order of two events that both clamp. The heap must be rebuilt.
 */
void test_step_clock_restores_heap_order_when_tags_clamp(void) {
  setup_scheduler();
  insert_system_event(1000, 5);
  insert_system_event(2000, 1);

  sched.super.step_clock(&sched.super, -SEC(1));

  tag_t expected[] = {{0, 1}, {0, 5}};
  assert_pops_in_order(expected, 2);
}

/** Stepping an empty queue must be harmless. */
void test_step_clock_on_empty_queue(void) {
  setup_scheduler();

  sched.super.step_clock(&sched.super, SEC(5));

  TEST_ASSERT_TRUE(system_event_queue.empty(&system_event_queue));
}

int main(void) {
  Environment_ctor(&env, NULL, NULL, false);
  env.platform = &mock_platform;

  UNITY_BEGIN();
  RUN_TEST(test_step_clock_shifts_pending_events_forward);
  RUN_TEST(test_step_clock_shifts_pending_events_backward);
  RUN_TEST(test_step_clock_restores_heap_order_when_tags_clamp);
  RUN_TEST(test_step_clock_on_empty_queue);
  return UNITY_END();
}
