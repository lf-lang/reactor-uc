#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

/* One small program shared by every extension test:
 *
 *   startup  -> on_startup   runs `at_startup`, the step a test needs at the start tag
 *   timer    -> on_timer     fires once, 5 ms after start
 *   action   -> on_action    deadline 1 ms, missed when `late` is set
 *               entry        has no trigger: only an extension can enqueue it
 *   shutdown -> on_shutdown
 *
 * The constructor binds `hooks`, which counts the extension hooks, and attaches a gate to
 * on_timer, to the timer and to the action. The gates are open unless a test closes them.
 */

static TestHooks hooks;
static void (*at_startup)(void* self);
static bool late;
static bool reaction_gate_open, timer_gate_open, action_gate_open;

static int timer_runs, action_runs, deadline_misses, entry_runs;
static instant_t timer_time;
static int action_values[4];
static instant_t action_times[4];
static tag_t entry_tag;
static TestHooks hooks_at_shutdown;

static LfGateCondition reaction_gate, timer_gate, action_gate;

LF_DEFINE_STARTUP_STRUCT(Ext, 1, 0);
LF_DEFINE_STARTUP_CTOR(Ext);
LF_DEFINE_SHUTDOWN_STRUCT(Ext, 1, 0);
LF_DEFINE_SHUTDOWN_CTOR(Ext);
LF_DEFINE_TIMER_STRUCT(Ext, timer, 1, 0);
LF_DEFINE_TIMER_CTOR(Ext, timer, 1, 0);
LF_DEFINE_ACTION_STRUCT(Ext, action, LogicalAction, 1, 1, 0, 4, int);
LF_DEFINE_ACTION_CTOR(Ext, action, LogicalAction, ACTION_POLICY_DEFER, 1, 1, 0, 4, int);
LF_DEFINE_REACTION_STRUCT(Ext, on_startup, 1);
LF_DEFINE_REACTION_CTOR(Ext, on_startup, 0, NULL, NULL);
LF_DEFINE_REACTION_STRUCT(Ext, on_timer, 0);
LF_DEFINE_REACTION_CTOR(Ext, on_timer, 1, NULL, NULL);
LF_DEFINE_REACTION_STRUCT(Ext, on_action, 0);
LF_DEFINE_REACTION_STRUCT(Ext, entry, 0);
LF_DEFINE_REACTION_CTOR(Ext, entry, 3, NULL, NULL);
LF_DEFINE_REACTION_STRUCT(Ext, on_shutdown, 0);
LF_DEFINE_REACTION_CTOR(Ext, on_shutdown, 4, NULL, NULL);

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Ext, on_startup);
  LF_REACTION_INSTANCE(Ext, on_timer);
  LF_REACTION_INSTANCE(Ext, on_action);
  LF_REACTION_INSTANCE(Ext, entry);
  LF_REACTION_INSTANCE(Ext, on_shutdown);
  LF_STARTUP_INSTANCE(Ext);
  LF_SHUTDOWN_INSTANCE(Ext);
  LF_TIMER_INSTANCE(Ext, timer);
  LF_ACTION_INSTANCE(Ext, action);
  LF_REACTOR_BOOKKEEPING_INSTANCES(5, 4, 0);
} Ext;

static instant_t elapsed(Environment* env) { return env->get_elapsed_logical_time(env); }

LF_DEFINE_REACTION_BODY(Ext, on_startup) {
  LF_SCOPE_SELF(Ext);
  if (at_startup != NULL) {
    at_startup(self);
  }
}

LF_DEFINE_REACTION_BODY(Ext, on_timer) {
  LF_SCOPE_SELF(Ext);
  LF_SCOPE_ENV();
  timer_time = elapsed(env);
  timer_runs++;
}

LF_DEFINE_REACTION_BODY(Ext, on_action) {
  LF_SCOPE_SELF(Ext);
  LF_SCOPE_ENV();
  LF_SCOPE_ACTION(Ext, action);
  action_values[action_runs] = action->value;
  action_times[action_runs] = elapsed(env);
  action_runs++;
}

LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(Ext, on_action) { deadline_misses++; }
LF_DEFINE_REACTION_CTOR(Ext, on_action, 2, LF_REACTION_TYPE(Ext, on_action_deadline_violation_handler), NULL);

LF_DEFINE_REACTION_BODY(Ext, entry) {
  LF_SCOPE_SELF(Ext);
  LF_SCOPE_ENV();
  entry_tag = env->scheduler->current_tag(env->scheduler);
  entry_runs++;
}

LF_DEFINE_REACTION_BODY(Ext, on_shutdown) { hooks_at_shutdown = hooks; }

static interval_t always_late(Environment* env) {
  (void)env;
  return MSEC(10);
}

LF_REACTOR_CTOR_SIGNATURE(Ext) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Ext);
  LF_INITIALIZE_REACTION(Ext, on_startup, NEVER);
  LF_INITIALIZE_REACTION(Ext, on_timer, NEVER);
  LF_INITIALIZE_REACTION(Ext, on_action, MSEC(1));
  LF_INITIALIZE_REACTION(Ext, entry, NEVER);
  LF_INITIALIZE_REACTION(Ext, on_shutdown, NEVER);
  LF_INITIALIZE_STARTUP(Ext);
  LF_INITIALIZE_SHUTDOWN(Ext);
  LF_INITIALIZE_TIMER(Ext, timer, MSEC(5), NEVER);
  LF_INITIALIZE_ACTION(Ext, action, MSEC(0), MSEC(0));
  LF_STARTUP_REGISTER_EFFECT(self->on_startup);
  LF_SHUTDOWN_REGISTER_EFFECT(self->on_shutdown);
  LF_TIMER_REGISTER_EFFECT(self->timer, self->on_timer);
  LF_ACTION_REGISTER_SOURCE(self->action, self->on_startup);
  LF_ACTION_REGISTER_EFFECT(self->action, self->on_action);

  if (late) {
    env->get_lag = always_late;
  }
  TEST_ASSERT_EQUAL(LF_OK, Environment_bind_extensions(env, &hooks));
  LfGateCondition_ctor(&reaction_gate);
  LfGateCondition_ctor(&timer_gate);
  LfGateCondition_ctor(&action_gate);
  TEST_ASSERT_EQUAL(LF_OK, LfReaction_add_gate(&self->on_timer.super, &reaction_gate, &reaction_gate_open));
  TEST_ASSERT_EQUAL(LF_OK, LfTimer_add_gate(&self->timer.super, &timer_gate, &timer_gate_open));
  TEST_ASSERT_EQUAL(LF_OK, LfAction_add_admission_gate(&self->action.super.super, &action_gate, &action_gate_open));
}

LF_ENTRY_POINT(Ext, 32, 32, FOREVER, false, true);

void setUp(void) {
  hooks = (TestHooks){0};
  hooks_at_shutdown = (TestHooks){0};
  at_startup = NULL;
  late = false;
  reaction_gate_open = timer_gate_open = action_gate_open = true;
  timer_runs = action_runs = deadline_misses = entry_runs = 0;
  timer_time = NEVER;
}

/* Hooks follow the program: the start tag, the timer's tag at 5 ms, then shutdown. */
static void test_lifecycle_hooks(void) {
  lf_start();
  TEST_ASSERT_EQUAL(3, hooks.tag_starts);
  TEST_ASSERT_EQUAL(1, hooks.shutdown_tag_starts);
  TEST_ASSERT_EQUAL(3, hooks.tag_completes);
  TEST_ASSERT_EQUAL(1, hooks.shutdowns);
  TEST_ASSERT_EQUAL(3, hooks.reaction_starts); // on_startup, on_timer, on_shutdown
  TEST_ASSERT_EQUAL(3, hooks.reaction_ends);
  // While on_shutdown runs, the shutdown tag has started but not completed.
  TEST_ASSERT_EQUAL(3, hooks_at_shutdown.tag_starts);
  TEST_ASSERT_EQUAL(2, hooks_at_shutdown.tag_completes);
  TEST_ASSERT_EQUAL(0, hooks_at_shutdown.shutdowns);
}

static void try_to_bind_and_add_a_gate(void* self) {
  Ext* ext = self;
  static LfGateCondition late_gate;
  LfGateCondition_ctor(&late_gate);
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, Environment_bind_extensions(ext->super.env, NULL));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, LfReaction_add_gate(&ext->entry.super, &late_gate, &reaction_gate_open));
}

/* Binding and gates are set up before the program runs, not while it runs. */
static void test_setup_is_fixed_once_running(void) {
  at_startup = try_to_bind_and_add_a_gate;
  lf_start();
}

static void request_next_microstep(void* self) {
  Ext* ext = self;
  Scheduler* scheduler = ext->super.env->scheduler;
  hooks.request_tag = true;
  hooks.requested_tag = lf_delay_tag(scheduler->current_tag(scheduler), 0);
  hooks.reaction_at_requested_tag = &ext->entry.super;
}

/* How MicroMode enters a mode: the extension asks for the next microstep, where nothing
 * is queued, and enqueues a reaction no trigger would run there. */
static void test_requested_tag_is_visited_without_an_event(void) {
  at_startup = request_next_microstep;
  lf_start();
  TEST_ASSERT_EQUAL(1, entry_runs);
  TEST_ASSERT_EQUAL(1, entry_tag.microstep);
  TEST_ASSERT_EQUAL(4, hooks.tag_starts); // start, the requested microstep, timer, shutdown
}

static void test_closed_reaction_gate_skips_the_reaction(void) {
  reaction_gate_open = false;
  lf_start();
  TEST_ASSERT_EQUAL(0, timer_runs);
  TEST_ASSERT_EQUAL(3, hooks.tag_starts); // The timer still fired.
}

static void test_closed_timer_gate_leaves_the_timer_unarmed(void) {
  timer_gate_open = false;
  lf_start();
  TEST_ASSERT_EQUAL(0, timer_runs);
  TEST_ASSERT_EQUAL(2, hooks.tag_starts); // Only start and shutdown.
}

static void schedule_one(void* self) {
  Ext* ext = self;
  lf_schedule(&ext->action, MSEC(1), 1);
}

static void test_closed_action_gate_drops_events(void) {
  action_gate_open = false;
  at_startup = schedule_one;
  lf_start();
  TEST_ASSERT_EQUAL(0, action_runs);
  TEST_ASSERT_EQUAL(1, hooks.outcomes[LF_SCHEDULE_DROPPED_GATE]);
}

static void test_gate_is_open_when_all_conditions_are_true(void) {
  LfGate gate;
  LfGateCondition a, b;
  bool a_value = true, b_value = false;
  LfGate_ctor(&gate);
  LfGateCondition_ctor(&a);
  LfGateCondition_ctor(&b);
  TEST_ASSERT_TRUE(LfGate_is_open(&gate)); // An empty gate is open.
  TEST_ASSERT_EQUAL(LF_OK, LfGate_add(&gate, &a, &a_value));
  TEST_ASSERT_EQUAL(LF_OK, LfGate_add(&gate, &b, &b_value));
  TEST_ASSERT_FALSE(LfGate_is_open(&gate));
  b_value = true;
  TEST_ASSERT_TRUE(LfGate_is_open(&gate));
}

static void take_restore_and_rearm(void* self) {
  Ext* ext = self;
  Trigger* action = (Trigger*)&ext->action;
  Event taken[3];
  size_t count = 0;

  // Purge drops a pending event: 99 is never delivered.
  lf_schedule(&ext->action, MSEC(1), 99);
  TEST_ASSERT_EQUAL(LF_OK, LfEventControl_purge(action, &count));
  TEST_ASSERT_EQUAL(1, count);

  // Take three events, discard the first and restore the others 10 ms later.
  lf_schedule(&ext->action, MSEC(1), 1);
  lf_schedule(&ext->action, MSEC(2), 2);
  lf_schedule(&ext->action, MSEC(3), 3);
  TEST_ASSERT_EQUAL(LF_OK, LfEventControl_take(action, taken, 3, &count));
  TEST_ASSERT_EQUAL(3, count);
  TEST_ASSERT_EQUAL(LF_OK, LfEventControl_discard(action, &taken[0], 1));
  TEST_ASSERT_EQUAL(LF_OK, LfEventControl_restore(action, &taken[1], 2, MSEC(10)));

  // Suspend the timer, due at 5 ms, and arm it again for 20 ms.
  interval_t remaining = 0;
  TEST_ASSERT_EQUAL(LF_OK, LfTimerControl_suspend(&ext->timer.super, &remaining));
  TEST_ASSERT_EQUAL(MSEC(5), remaining);
  TEST_ASSERT_EQUAL(LF_OK, LfTimerControl_arm(&ext->timer.super, MSEC(20)));
}

/* How MicroMode suspends the triggers of an inactive mode and resumes them later. */
static void test_taken_events_are_restored_or_discarded(void) {
  at_startup = take_restore_and_rearm;
  lf_start();
  TEST_ASSERT_EQUAL(2, action_runs);
  TEST_ASSERT_EQUAL(2, action_values[0]);
  TEST_ASSERT_EQUAL(MSEC(12), action_times[0]);
  TEST_ASSERT_EQUAL(3, action_values[1]);
  TEST_ASSERT_EQUAL(MSEC(13), action_times[1]);
  TEST_ASSERT_EQUAL(1, timer_runs);
  TEST_ASSERT_EQUAL(MSEC(20), timer_time);
}

static void schedule_five(void* self) {
  Ext* ext = self;
  for (int i = 1; i <= 5; i++) {
    lf_schedule(&ext->action, MSEC(i), i);
  }
}

/* The action holds four pending events, so the fifth request is refused. */
static void test_schedule_outcomes_are_reported(void) {
  at_startup = schedule_five;
  lf_start();
  TEST_ASSERT_EQUAL(4, hooks.outcomes[LF_SCHEDULE_ADMITTED]);
  TEST_ASSERT_EQUAL(1, hooks.outcomes[LF_SCHEDULE_BUFFER_FULL]);
  TEST_ASSERT_EQUAL(4, action_runs);
}

/* A missed deadline is reported, and its handler is not a reaction body. */
static void test_deadline_violation_is_reported(void) {
  late = true;
  at_startup = schedule_one;
  lf_start();
  TEST_ASSERT_EQUAL(0, action_runs);
  TEST_ASSERT_EQUAL(1, deadline_misses);
  TEST_ASSERT_EQUAL(1, hooks.violations);
  TEST_ASSERT_EQUAL(3, hooks.reaction_starts); // on_startup, on_timer, on_shutdown
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_lifecycle_hooks);
  RUN_TEST(test_setup_is_fixed_once_running);
  RUN_TEST(test_requested_tag_is_visited_without_an_event);
  RUN_TEST(test_closed_reaction_gate_skips_the_reaction);
  RUN_TEST(test_closed_timer_gate_leaves_the_timer_unarmed);
  RUN_TEST(test_closed_action_gate_drops_events);
  RUN_TEST(test_gate_is_open_when_all_conditions_are_true);
  RUN_TEST(test_taken_events_are_restored_or_discarded);
  RUN_TEST(test_schedule_outcomes_are_reported);
  RUN_TEST(test_deadline_violation_is_reported);
  return UNITY_END();
}
