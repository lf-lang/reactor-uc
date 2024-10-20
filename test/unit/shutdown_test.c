
#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_STARTUP_STRUCT(MyStartup, 1)
DEFINE_SHUTDOWN_STRUCT(MyShutdown, 1)
DEFINE_STARTUP_CTOR(MyStartup, 1)
DEFINE_SHUTDOWN_CTOR(MyShutdown, 1)
DEFINE_REACTION_STRUCT(MyReactor, 0, 0)
DEFINE_REACTION_STRUCT(MyReactor, 1, 0)

typedef struct {
  Reactor super;
  MyReactor_Reaction0 reaction0;
  MyReactor_Reaction1 reaction1;
  MyStartup startup;
  MyShutdown shutdown;
  Reaction *_reactions[2];
  Trigger *_triggers[2];
} MyReactor;

DEFINE_REACTION_BODY(MyReactor, 0) { printf("Startup reaction executing\n"); }

DEFINE_REACTION_BODY(MyReactor, 1) { printf("Shutdown reaction executing\n"); }

DEFINE_REACTION_CTOR(MyReactor, 0)
DEFINE_REACTION_CTOR(MyReactor, 1)

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction0;
  self->_reactions[1] = (Reaction *)&self->reaction1;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->shutdown;

  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 2, self->_triggers, 2);
  MyReactor_Reaction0_ctor(&self->reaction0, &self->super);
  MyReactor_Reaction1_ctor(&self->reaction1, &self->super);
  MyStartup_ctor(&self->startup, &self->super);
  MyShutdown_ctor(&self->shutdown, &self->super);

  BUILTIN_REGISTER_EFFECT(self->startup, self->reaction0);
  BUILTIN_REGISTER_EFFECT(self->shutdown, self->reaction1);
}

ENTRY_POINT(MyReactor)

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}
