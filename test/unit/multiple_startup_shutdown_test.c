#include "reactor-uc/reactor-uc.h"
#include "unity.h"

DEFINE_STARTUP_STRUCT(ShutdownTest, 1)
DEFINE_STARTUP_CTOR(ShutdownTest)
DEFINE_REACTION_STRUCT(ShutdownTest, r_startup, 0)
DEFINE_REACTION_CTOR(ShutdownTest, r_startup, 0)

DEFINE_SHUTDOWN_STRUCT(ShutdownTest, 1)
DEFINE_SHUTDOWN_CTOR(ShutdownTest)
DEFINE_REACTION_STRUCT(ShutdownTest, r_shutdown, 0)
DEFINE_REACTION_CTOR(ShutdownTest, r_shutdown, 1)


typedef struct {
  Reactor super;
  REACTION_INSTANCE(ShutdownTest, r_startup);
  REACTION_INSTANCE(ShutdownTest, r_shutdown);
  STARTUP_INSTANCE(ShutdownTest);
  SHUTDOWN_INSTANCE(ShutdownTest);
  Reaction *_reactions[2];
  Trigger *_triggers[2];
  int cnt;
} ShutdownTest;

DEFINE_REACTION_BODY(ShutdownTest, r_startup) {
  SCOPE_SELF(ShutdownTest);
}

DEFINE_REACTION_BODY(ShutdownTest, r_shutdown) {
  SCOPE_SELF(ShutdownTest);
}

void ShutdownTest_ctor(ShutdownTest *self, Environment *env) {
  Reactor_ctor(&self->super, "ShutdownTest", env, NULL, NULL, 0, self->_reactions, 2, self->_triggers, 2);
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_STARTUP(ShutdownTest);
  INITIALIZE_REACTION(ShutdownTest, r_startup);
  STARTUP_REGISTER_EFFECT(r_startup);

  INITIALIZE_SHUTDOWN(ShutdownTest);
  INITIALIZE_REACTION(ShutdownTest, r_shutdown);
  SHUTDOWN_REGISTER_EFFECT(r_shutdown);
}

typedef struct {
  Reactor super;
  ShutdownTest reactors[2];
  Reactor *children[2];
} MultipleShutdownTest;


void MultipleShutdownTest_ctor(MultipleShutdownTest *self, Environment *env) {
  Reactor_ctor(&self->super, "MultipleShutdownTest", env, NULL, self->children, 2, NULL, 0, NULL, 0);
  self->children[0] = (Reactor *)&self->reactors[0];
  self->children[1] = (Reactor *)&self->reactors[1];
  ShutdownTest_ctor(&self->reactors[0], env);
  ShutdownTest_ctor(&self->reactors[1], env);

}

ENTRY_POINT(MultipleShutdownTest, FOREVER, false);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}