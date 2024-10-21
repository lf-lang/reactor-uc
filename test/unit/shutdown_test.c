
#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_STARTUP(MyStartup, 1)
DEFINE_SHUTDOWN(MyShutdown, 1)
DEFINE_REACTION(MyReactor, 0, 0)
DEFINE_REACTION(MyReactor, 1, 0)

typedef struct {
  Reactor super;
  MyReactor_0 reaction0;
  MyReactor_1 reaction1;
  MyStartup startup;
  MyShutdown shutdown;
  Reaction *_reactions[2];
  Trigger *_triggers[2];
} MyReactor;

REACTION_BODY(MyReactor, 0, { printf("Startup reaction executing\n"); })

REACTION_BODY(MyReactor, 1, { printf("Shutdown reaction executing\n"); })

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction0;
  self->_reactions[1] = (Reaction *)&self->reaction1;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->shutdown;

  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 2, self->_triggers, 2);
  MyReactor_0_ctor(&self->reaction0, &self->super);
  MyReactor_1_ctor(&self->reaction1, &self->super);
  MyStartup_ctor(&self->startup, &self->super);
  MyShutdown_ctor(&self->shutdown, &self->super);

  STARTUP_REGISTER_EFFECT(self->startup, self->reaction0);
  SHUTDOWN_REGISTER_EFFECT(self->shutdown, self->reaction1);
}

ENTRY_POINT(MyReactor, FOREVER, false)

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}
