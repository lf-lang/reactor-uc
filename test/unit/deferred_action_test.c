#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

// Tests the level-deferred actions of the dynamic scheduler (used to flush federated outputs).

#define QUEUE_SIZE 8

static Environment env;
Environment* _lf_environment = &env; // NOLINT

static instant_t mock_get_physical_time(Platform* self) {
  (void)self;
  return 0;
}
static Platform mock_platform = {.get_physical_time = mock_get_physical_time}; // used by the logging

static DynamicScheduler sched;
static EventQueue event_queue;
static ReactionQueue reaction_queue;
static ArbitraryEvent event_queue_array[QUEUE_SIZE];
static Reaction* reaction_queue_level_tail[QUEUE_SIZE];
static lf_level_word_t reaction_queue_level_occupied[LF_LEVEL_WORDS(QUEUE_SIZE)];
static Reactor parent;

// Execution log: reactions log 'a' + index, deferred actions 'A' + index.
static char exec_log[32];
static size_t log_len;

static Reaction reactions[3];
static LevelDeferredAction actions[4];

static void log_char(char c) {
  TEST_ASSERT_LESS_THAN(sizeof(exec_log) - 1, log_len);
  exec_log[log_len++] = c;
  exec_log[log_len] = '\0';
}

static void action_execute(LevelDeferredAction* action) { log_char((char)('A' + (action - actions))); }

static void defer(size_t action_idx, int level) {
  actions[action_idx].level = level;
  sched.super.defer_until_level_done(&sched.super, &actions[action_idx]);
}

// Reaction 0 (level 0): defers A (level 0) twice and C (level 1).
static void reaction0_body(Reaction* self) {
  (void)self;
  log_char('a');
  defer(0, 0);
  defer(0, 0); // already pending: must be executed only once
  defer(2, 1);
}

// Reaction 1 (level 1): defers B (level 1), which must run after C (FIFO within a level).
static void reaction1_body(Reaction* self) {
  (void)self;
  log_char('b');
  defer(1, 1);
}

// Reaction 2 (level 2): defers D (level 2), which can only run at the end of the tag.
static void reaction2_body(Reaction* self) {
  (void)self;
  log_char('c');
  defer(3, 2);
}

static void setup(void) {
  env.platform = &mock_platform;
  EventQueue_ctor(&event_queue, event_queue_array, QUEUE_SIZE);
  ReactionQueue_ctor(&reaction_queue, reaction_queue_level_tail, reaction_queue_level_occupied, QUEUE_SIZE);
  DynamicScheduler_ctor(&sched, &env, &event_queue, NULL, &reaction_queue, FOREVER, false);
  Reactor_ctor(&parent, "parent", &env, NULL, NULL, 0, NULL, 0, NULL, 0);

  void (*bodies[3])(Reaction*) = {reaction0_body, reaction1_body, reaction2_body};
  for (size_t i = 0; i < 3; i++) {
    Reaction_ctor(&reactions[i], &parent, bodies[i], NULL, 0, i, NULL, 0, NULL);
    reactions[i].level = (int)i;
  }
  for (size_t i = 0; i < 4; i++) {
    actions[i].next = NULL;
    actions[i].pending = false;
    actions[i].execute = action_execute;
  }
  log_len = 0;
  exec_log[0] = '\0';
}

static void run_tag(const Reaction* const* triggered, size_t n) {
  reaction_queue.reset(&reaction_queue);
  for (size_t i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL(LF_OK, reaction_queue.insert(&reaction_queue, (Reaction*)triggered[i]));
  }
  sched.run_timestep(&sched.super);
  TEST_ASSERT_NULL(sched.deferred_head);
  for (size_t i = 0; i < 4; i++) {
    TEST_ASSERT_FALSE(actions[i].pending);
  }
}

/** Deferred actions run before the first reaction of a higher level, in level order and FIFO within a level. */
void test_executed_at_level_boundaries(void) {
  setup();
  const Reaction* triggered[] = {&reactions[2], &reactions[0], &reactions[1]};
  run_tag(triggered, 3);
  TEST_ASSERT_EQUAL_STRING("aAbCBcD", exec_log);
}

/** Deferred actions whose level has no later reaction run at the end of the tag. */
void test_executed_at_end_of_tag(void) {
  setup();
  const Reaction* triggered[] = {&reactions[0]};
  run_tag(triggered, 1);
  TEST_ASSERT_EQUAL_STRING("aAC", exec_log);
}

/** After a tag, an action can be deferred again (once per tag). */
void test_once_per_tag(void) {
  setup();
  const Reaction* triggered[] = {&reactions[0]};
  run_tag(triggered, 1);
  run_tag(triggered, 1);
  TEST_ASSERT_EQUAL_STRING("aACaAC", exec_log);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_executed_at_level_boundaries);
  RUN_TEST(test_executed_at_end_of_tag);
  RUN_TEST(test_once_per_tag);
  return UNITY_END();
}
