#include "reactor-uc/reactor-uc.h"

#include "unity.h"

DEFINE_STARTUP_STRUCT(MyStartup, 1)
DEFINE_STARTUP_CTOR(MyStartup, 1)
DEFINE_REACTION_STRUCT(MyReactor, 0, 0)

typedef struct {
  Reactor super;
  MyReactor_Reaction0 my_reaction;
  MyStartup startup;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
  int cnt;
} MyReactor;

DEFINE_REACTION_BODY(MyReactor, 0, { printf("Hello World\n"); })
DEFINE_REACTION_CTOR(MyReactor, 0)

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReactor_Reaction0_ctor(&self->my_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super);

  BUILTIN_REGISTER_EFFECT(self->startup, self->my_reaction);
}

ENTRY_POINT(MyReactor);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}