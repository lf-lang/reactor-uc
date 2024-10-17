
#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_STARTUP(MyStartup, 1)
DEFINE_SHUTDOWN(MyShutdown, 1)
DEFINE_REACTION(Reaction1, 0)
DEFINE_REACTION(Reaction2, 0)

typedef struct {
  Reactor super;
  Reaction1 reaction1;
  Reaction2 reaction2;
  MyStartup startup;
  MyShutdown shutdown;
  Reaction *_reactions[2];
  Trigger *_triggers[2];
} MyReactor;

CONSTRUCTOR_REACTION(Reaction1, MyReactor, 0, {
  printf("Startup reaction executing\n");
})

CONSTRUCTOR_REACTION(Reaction2, MyReactor, 1, {
  printf("Shutdown reaction executing\n");
})

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction1;
  self->_reactions[1] = (Reaction *)&self->reaction2;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->shutdown;

  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 2, self->_triggers, 2);
  Reaction1_ctor(&self->reaction1, &self->super);
  Reaction2_ctor(&self->reaction2, &self->super);
  MyStartup_ctor(&self->startup, &self->super);
  MyShutdown_ctor(&self->shutdown, &self->super);

  STARTUP_REGISTER_EFFECT(self->startup, self->reaction1);
  SHUTDOWN_REGISTER_EFFECT(self->shutdown, self->reaction2);
}

void test_simple() {
  MyReactor my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}
