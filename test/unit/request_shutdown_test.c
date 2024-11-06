#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_ACTION_STRUCT(MyAction, LOGICAL_ACTION, 1, 1, 2, int);
DEFINE_ACTION_CTOR(MyAction, LOGICAL_ACTION, MSEC(0), 1, 1, 2, int);
DEFINE_STARTUP_STRUCT(MyStartup, 1);
DEFINE_STARTUP_CTOR(MyStartup, 1)
DEFINE_SHUTDOWN_STRUCT(MyShutdown, 1);
DEFINE_SHUTDOWN_CTOR(MyShutdown, 1)
DEFINE_REACTION_STRUCT(MyReactor, 0, 1);
DEFINE_REACTION_STRUCT(MyReactor, 1, 0);

typedef struct {
  Reactor super;
  MyReactor_Reaction0 my_reaction;
  MyReactor_Reaction1 my_reaction1;
  MyAction my_action;
  MyStartup startup;
  MyShutdown shutdown;
  Reaction *_reactions[2];
  Trigger *_triggers[3];
  int cnt;
  tag_t last_tag;
} MyReactor;

DEFINE_REACTION_BODY(MyReactor, 0) {
  MyReactor *self = (MyReactor *)_self->parent;
  MyAction *my_action = &self->my_action;
  Environment *env = self->super.env;

  if (env->get_elapsed_logical_time(env) == MSEC(1)) {
    TEST_ASSERT_EQUAL(2, self->cnt);
    env->request_shutdown(env);
    self->last_tag = env->scheduler.current_tag;
    TEST_ASSERT_EQUAL(env->scheduler.stop_tag.time, self->last_tag.time);
    TEST_ASSERT_EQUAL(env->scheduler.stop_tag.microstep, self->last_tag.microstep + 1);
  }

  lf_schedule(my_action, MSEC(1), ++self->cnt);
  lf_schedule(my_action, MSEC(2), ++self->cnt);
}
DEFINE_REACTION_CTOR(MyReactor, 0);

DEFINE_REACTION_BODY(MyReactor, 1) {
  MyReactor *self = (MyReactor *)_self->parent;
  Environment *env = self->super.env;
  TEST_ASSERT_EQUAL(4, self->cnt);
  TEST_ASSERT_EQUAL(env->scheduler.current_tag.time, self->last_tag.time);
  TEST_ASSERT_EQUAL(env->scheduler.current_tag.microstep, self->last_tag.microstep + 1);
}

DEFINE_REACTION_CTOR(MyReactor, 1);

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_reactions[1] = (Reaction *)&self->my_reaction1;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->my_action;
  self->_triggers[2] = (Trigger *)&self->shutdown;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 2, self->_triggers, 3);
  MyAction_ctor(&self->my_action, &self->super);
  MyReactor_Reaction0_ctor(&self->my_reaction, &self->super);
  MyReactor_Reaction1_ctor(&self->my_reaction1, &self->super);
  MyStartup_ctor(&self->startup, &self->super);
  MyShutdown_ctor(&self->shutdown, &self->super);
  ACTION_REGISTER_EFFECT(self->my_action, self->my_reaction);
  REACTION_REGISTER_EFFECT(self->my_reaction, self->my_action);
  ACTION_REGISTER_SOURCE(self->my_action, self->my_reaction);
  BUILTIN_REGISTER_EFFECT(self->startup, self->my_reaction);
  BUILTIN_REGISTER_EFFECT(self->shutdown, self->my_reaction1);

  self->cnt = 0;
}

void test_simple() {
  MyReactor my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
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