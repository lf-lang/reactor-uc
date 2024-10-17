#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_LOGICAL_ACTION(MyAction, 1, 1, int, 2);
DEFINE_STARTUP(MyStartup, 1);
DEFINE_REACTION(MyReaction, 1);

typedef struct {
  Reactor super;
  MyReaction my_reaction;
  MyAction my_action;
  MyStartup startup;
  Reaction *_reactions[1];
  Trigger *_triggers[2];
  int cnt;
} MyReactor ;

void MyAction_ctor(MyAction *self, MyReactor *parent) {
  LogicalAction_ctor(&self->super, MSEC(0), MSEC(0), &parent->super, self->sources, 1, self->effects, 1, &self->buffer,
                     sizeof(self->buffer[0]), 2);
}

CONSTRUCT_STARTUP(MyStartup, MyReactor)

CONSTRUCT_REACTION(MyReaction, MyReactor, 0, {
  MyAction *my_action = &self->my_action;
  if (self->cnt == 0) {
    TEST_ASSERT_EQUAL(lf_is_present(my_action), false);
  } else {
    TEST_ASSERT_EQUAL(lf_is_present(my_action), true);
  }

  printf("Hello World\n");
  printf("Action = %d\n", lf_get(my_action));
  if (self->cnt > 0) {
    TEST_ASSERT_EQUAL(self->cnt, lf_get(my_action));
  }

  lf_schedule(my_action, ++self->cnt, MSEC(100));
});

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->my_action;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 2);
  MyAction_ctor(&self->my_action, self);
  MyReaction_ctor(&self->my_reaction, self);
  MyStartup_ctor(&self->startup, self);
  ACTION_REGISTER_EFFECT(self->my_action, self->my_reaction);
  REACTION_REGISTER_EFFECT(self->my_reaction, self->my_action);
  ACTION_REGISTER_SOURCE(self->my_action, self->my_reaction);
  STARTUP_REGISTER_EFFECT(self->startup, self->my_action);

  self->cnt = 0;
}

void test_simple() {
  MyReactor my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.scheduler.set_timeout(&env.scheduler, SEC(1));
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}