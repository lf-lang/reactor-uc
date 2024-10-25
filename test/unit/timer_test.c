#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_TIMER_STRUCT(MyTimer, 1)
DEFINE_TIMER_CTOR_FIXED(MyTimer, 1, 0, MSEC(1))
DEFINE_REACTION_STRUCT(MyReactor, 0, 0)

typedef struct {
  Reactor super;
  MyReactor_Reaction0 my_reaction;
  MyTimer timer;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
  int cnt;
} MyReactor;

DEFINE_REACTION_BODY(MyReactor, 0) {
  MyReactor *self = (MyReactor *)_self->parent;
  Environment *env = self->super.env;
  TEST_ASSERT_EQUAL(self->cnt * MSEC(1), env->get_elapsed_logical_time(env));
  printf("Hello World @ %ld\n", env->get_elapsed_logical_time(env));
  self->cnt++;
}

DEFINE_REACTION_CTOR(MyReactor, 0)
void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  self->cnt = 0;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReactor_Reaction0_ctor(&self->my_reaction, &self->super);
  MyTimer_ctor(&self->timer, &self->super);
  TIMER_REGISTER_EFFECT(self->timer, self->my_reaction);
}

MyReactor my_reactor;
Environment env;
void test_simple() {
  Environment_ctor(&env, (Reactor *)&my_reactor);
  env.scheduler.set_duration(&env.scheduler, MSEC(100));
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}
