#include "reactor-uc/reactor-uc.h"

#include "unity.h"

#include "action_lib.h"

DEFINE_REACTION_BODY(ActionLib, reaction) {
  SCOPE_SELF(ActionLib);
  SCOPE_ENV();
  SCOPE_ACTION(ActionLib, act);

  if (env->get_elapsed_logical_time(env) == MSEC(1)) {
    TEST_ASSERT_EQUAL(2, self->cnt);
    env->request_shutdown(env);
  }

  lf_schedule(act, MSEC(1), ++self->cnt);
  lf_schedule(act, MSEC(2), ++self->cnt);
}

DEFINE_REACTION_BODY(ActionLib, r_shutdown) {
}

void test_run() {
  action_int_lib_start(MSEC(100));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_run);
  return UNITY_END();
}

DEFINE_REACTION_BODY(MyReactor, 0) {
  MyReactor *self = (MyReactor *)_self->parent;
  MyAction *my_action = &self->my_action;
  Environment *env = self->super.env;

}
DEFINE_REACTION_CTOR(MyReactor, 0);

DEFINE_REACTION_BODY(MyReactor, 1) {
  MyReactor *self = (MyReactor *)_self->parent;
  Environment *env = self->super.env;
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