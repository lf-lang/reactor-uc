#include "reactor-uc/reactor-uc.h"

#include "unity.h"

DEFINE_STARTUP(MyStartup, 1)
DEFINE_REACTION(MyReactor, 0, 0)

typedef struct {
  Reactor super;
  MyReactor_0 my_reaction;
  MyStartup startup;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
  int cnt;
} MyReactor;

REACTION_BODY(MyReactor, 0, { printf("Hello World\n"); })

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->startup;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReactor_0_ctor(&self->my_reaction, &self->super);
  MyStartup_ctor(&self->startup, &self->super);

  STARTUP_REGISTER_EFFECT(self->startup, self->my_reaction);
}

ENTRY_POINT(MyReactor, FOREVER, false);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}