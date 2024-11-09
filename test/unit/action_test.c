#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_ACTION_STRUCT(ActionTest, act, LOGICAL_ACTION, 1, 1, 2, int);
DEFINE_ACTION_CTOR(ActionTest, act, LOGICAL_ACTION, 1, 1, 2, int);
DEFINE_STARTUP_STRUCT(ActionTest, 1);
DEFINE_STARTUP_CTOR(ActionTest);
DEFINE_REACTION_STRUCT(ActionTest, r0, 1);
DEFINE_REACTION_CTOR(ActionTest, r0, 0);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(ActionTest, r0);
  ACTION_INSTANCE(ActionTest, act);
  STARTUP_INSTANCE(ActionTest);
  Reaction *_reactions[1];
  Trigger *_triggers[2];
  int cnt;
} ActionTest;

DEFINE_REACTION_BODY(ActionTest, r0) {
  SCOPE_SELF(ActionTest);
  SCOPE_ACTION(ActionTest, act);

  if (self->cnt == 0) {
    // First triggering is from startup reaction, and action should be false.
    TEST_ASSERT_EQUAL(lf_is_present(act), false);
  } else {
    // Rest of triggers are from action.
    TEST_ASSERT_EQUAL(lf_is_present(act), true);
  }

  printf("Hello World\n");
  printf("Action = %d\n", act->value);
  if (self->cnt > 0) {
    // The value of the event should be equal to the count.
    TEST_ASSERT_EQUAL(self->cnt, act->value);
  }

  // Schedule count and increment.
  lf_schedule(act, ++self->cnt, MSEC(1));
}

void ActionTest_ctor(ActionTest *self, Environment *env) {
  Reactor_ctor(&self->super, "ActionTest", env, NULL, NULL, 0, self->_reactions,
               sizeof(self->_reactions) / sizeof(self->_reactions[0]), self->_triggers,
               sizeof(self->_triggers) / sizeof(self->_triggers[0]));
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_REACTION(ActionTest, r0);
  INITIALIZE_ACTION(ActionTest, act, MSEC(0));
  INITIALIZE_STARTUP(ActionTest);
  ACTION_REGISTER_EFFECT(act, r0);
  REACTION_REGISTER_EFFECT(r0, act);
  ACTION_REGISTER_SOURCE(act, r0);
  STARTUP_REGISTER_EFFECT(r0);

  self->cnt = 0;
}

void test_simple() {
  ActionTest my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  ActionTest_ctor(&my_reactor, &env);
  env.scheduler.duration = MSEC(100);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}