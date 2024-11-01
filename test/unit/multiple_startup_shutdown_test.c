
#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_STARTUP_STRUCT(MyStartup, 1)
DEFINE_STARTUP_CTOR(MyStartup, 1)

DEFINE_STARTUP_STRUCT(MyStartup2, 1)
DEFINE_STARTUP_CTOR(MyStartup2, 1)

DEFINE_SHUTDOWN_STRUCT(MyShutdown, 1)
DEFINE_SHUTDOWN_CTOR(MyShutdown, 1)

DEFINE_SHUTDOWN_STRUCT(MyShutdown2, 1)
DEFINE_SHUTDOWN_CTOR(MyShutdown2, 1)

DEFINE_REACTION_STRUCT(MyReactor, 0, 0)
DEFINE_REACTION_STRUCT(MyReactor, 1, 0)
DEFINE_REACTION_STRUCT(MyReactor, 2, 0)
DEFINE_REACTION_STRUCT(MyReactor, 3, 0)

typedef struct {
  Reactor super;
  MyReactor_Reaction0 reaction0;
  MyReactor_Reaction1 reaction1;
  MyReactor_Reaction2 reaction2;
  MyReactor_Reaction3 reaction3;
  MyStartup startup;
  MyStartup2 startup2;
  MyShutdown shutdown;
  MyShutdown2 shutdown2;
  Reaction *_reactions[4];
  Trigger *_triggers[4];
  int cnt;
} MyReactor;

DEFINE_REACTION_BODY(MyReactor, 0) {
  MyReactor *self = (MyReactor *)_self->parent;
  TEST_ASSERT_EQUAL(0, self->cnt++);
}

DEFINE_REACTION_BODY(MyReactor, 1) {
  MyReactor *self = (MyReactor *)_self->parent;
  TEST_ASSERT_EQUAL(1, self->cnt++);
}

DEFINE_REACTION_BODY(MyReactor, 2) {
  MyReactor *self = (MyReactor *)_self->parent;
  TEST_ASSERT_EQUAL(2, self->cnt++);
}

DEFINE_REACTION_BODY(MyReactor, 3) {
  MyReactor *self = (MyReactor *)_self->parent;
  TEST_ASSERT_EQUAL(3, self->cnt++);
}

DEFINE_REACTION_CTOR(MyReactor, 0)
DEFINE_REACTION_CTOR(MyReactor, 1)
DEFINE_REACTION_CTOR(MyReactor, 2)
DEFINE_REACTION_CTOR(MyReactor, 3)

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction0;
  self->_reactions[1] = (Reaction *)&self->reaction1;
  self->_reactions[2] = (Reaction *)&self->reaction2;
  self->_reactions[3] = (Reaction *)&self->reaction3;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->shutdown;
  self->_triggers[2] = (Trigger *)&self->startup2;
  self->_triggers[3] = (Trigger *)&self->shutdown2;

  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 4, self->_triggers, 4);
  MyReactor_Reaction0_ctor(&self->reaction0, &self->super);
  MyReactor_Reaction1_ctor(&self->reaction1, &self->super);
  MyReactor_Reaction2_ctor(&self->reaction2, &self->super);
  MyReactor_Reaction3_ctor(&self->reaction3, &self->super);

  MyStartup_ctor(&self->startup, &self->super);
  MyShutdown_ctor(&self->shutdown, &self->super);
  MyStartup2_ctor(&self->startup2, &self->super);
  MyShutdown2_ctor(&self->shutdown2, &self->super);

  BUILTIN_REGISTER_EFFECT(self->startup, self->reaction0);
  BUILTIN_REGISTER_EFFECT(self->startup2, self->reaction1);
  BUILTIN_REGISTER_EFFECT(self->shutdown, self->reaction2);
  BUILTIN_REGISTER_EFFECT(self->shutdown2, self->reaction3);
}

ENTRY_POINT(MyReactor, FOREVER, false)

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}
