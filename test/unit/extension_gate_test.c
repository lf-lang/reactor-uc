#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

static bool gate_open = false;
static int body_calls = 0;
static int deadline_calls = 0;

LF_DEFINE_TIMER_STRUCT(GateTest, t, 2, 0)
LF_DEFINE_TIMER_CTOR(GateTest, t, 2, 0)
LF_DEFINE_ACTION_STRUCT(GateTest, a, LogicalAction, 1, 1, 0, 4, int)
LF_DEFINE_ACTION_CTOR(GateTest, a, LogicalAction, ACTION_POLICY_DEFER, 1, 1, 0, 4, int)
LF_DEFINE_REACTION_STRUCT(GateTest, r_gated, 0)
LF_DEFINE_REACTION_STRUCT(GateTest, r_drive, 1)

LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(GateTest, r_gated);
LF_DEFINE_REACTION_CTOR(GateTest, r_gated, 0, LF_REACTION_TYPE(GateTest, r_gated_deadline_violation_handler), NULL)
LF_DEFINE_REACTION_CTOR(GateTest, r_drive, 1, NULL, NULL)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(GateTest, r_gated);
  LF_REACTION_INSTANCE(GateTest, r_drive);
  LF_TIMER_INSTANCE(GateTest, t);
  LF_ACTION_INSTANCE(GateTest, a);
  LF_REACTOR_BOOKKEEPING_INSTANCES(2, 2, 0);
} GateTest;

LF_DEFINE_REACTION_BODY(GateTest, r_gated) {
  (void)_self;
  body_calls++;
}

LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(GateTest, r_gated) {
  (void)_self;
  deadline_calls++;
}

LF_DEFINE_REACTION_BODY(GateTest, r_drive) {
  LF_SCOPE_SELF(GateTest);
  LF_SCOPE_ACTION(GateTest, a);
  lf_schedule(a, 1, MSEC(1)); // keeps the action busy while the gate is closed
}

LF_REACTOR_CTOR_SIGNATURE(GateTest) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(GateTest);
  LF_INITIALIZE_REACTION(GateTest, r_gated, MSEC(0)); // deadline 0: always violated
  LF_INITIALIZE_REACTION(GateTest, r_drive, NEVER);
  LF_INITIALIZE_TIMER(GateTest, t, MSEC(0), MSEC(1));
  LF_INITIALIZE_ACTION(GateTest, a, MSEC(0), MSEC(0));
  LF_TIMER_REGISTER_EFFECT(self->t, self->r_gated);
  LF_TIMER_REGISTER_EFFECT(self->t, self->r_drive);
  LF_ACTION_REGISTER_EFFECT(self->a, self->r_gated);
  LF_ACTION_REGISTER_SOURCE(self->a, self->r_drive);
  LF_REACTION_SET_GATE(r_gated, &gate_open);
}

LF_ENTRY_POINT(GateTest, 32, 32, MSEC(5), false, true)

static size_t payload_slots_in_use(const EventPayloadPool* pool) {
  size_t in_use = 0;
  for (size_t idx = 0; idx < pool->capacity; idx++) {
    if (pool->used[idx]) {
      in_use++;
    }
  }
  return in_use;
}

void test_gate_blocks_body_deadline_and_budget(void) {
  TEST_ASSERT_EQUAL_MESSAGE(0, body_calls, "a gated-off reaction must never run");
  TEST_ASSERT_EQUAL_MESSAGE(0, deadline_calls, "a gated-off reaction must never reach the deadline check");

  TEST_ASSERT_EQUAL_MESSAGE(0, main_reactor.a.super.super.events_scheduled,
                            "a gated-off action must not leak its pending-event budget");
  TEST_ASSERT_EQUAL_size_t_MESSAGE(0, payload_slots_in_use(&main_reactor.a.super.super.payload_pool),
                                   "nor any payload-pool slot -- the \"full payload pool\" half");
}

void test_gate_conditions_compose_and_have_one_owner(void) {
  bool first_open = true;
  bool second_open = false;
  LfGate gate;
  LfGate other;
  LfGateCondition first;
  LfGateCondition second;
  LfGate_ctor(&gate);
  LfGate_ctor(&other);
  LfGateCondition_ctor(&first);
  LfGateCondition_ctor(&second);

  TEST_ASSERT_EQUAL(LF_OK, LfGate_add(&gate, &first, &first_open));
  TEST_ASSERT_EQUAL(LF_OK, LfGate_add(&gate, &second, &second_open));
  TEST_ASSERT_FALSE(LfGate_is_open(&gate));
  second_open = true;
  TEST_ASSERT_TRUE(LfGate_is_open(&gate));
  TEST_ASSERT_TRUE(LfGate_contains(&gate, &first_open));
  TEST_ASSERT_EQUAL(LF_OK, LfGate_add(&gate, &first, &first_open));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, LfGate_add(&gate, &first, &second_open));
  TEST_ASSERT_TRUE(LfGate_contains(&gate, &first_open));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, LfGate_add(&other, &first, &first_open));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  RUN_TEST(test_gate_blocks_body_deadline_and_budget);
  RUN_TEST(test_gate_conditions_compose_and_have_one_owner);
  return UNITY_END();
}
