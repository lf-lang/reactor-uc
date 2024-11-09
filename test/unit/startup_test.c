#include "reactor-uc/reactor-uc.h"

#include "unity.h"

DEFINE_STARTUP_STRUCT(StartupTest, 1)
DEFINE_STARTUP_CTOR(StartupTest)
DEFINE_REACTION_STRUCT(StartupTest, r_startup, 0)
DEFINE_REACTION_CTOR(StartupTest, r_startup, 0)

typedef struct {
  Reactor super;
  REACTION_INSTANCE(StartupTest, r_startup);
  STARTUP_INSTANCE(StartupTest);  
  Reaction *_reactions[1];
  Trigger *_triggers[1];
  int cnt;
} StartupTest;

DEFINE_REACTION_BODY(StartupTest, r_startup) { printf("Hello World\n"); }

void StartupTest_ctor(StartupTest *self, Environment *env) {
  Reactor_ctor(&self->super, "StartupTest", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_STARTUP(StartupTest);
  INITIALIZE_REACTION(StartupTest, r_startup);
  STARTUP_REGISTER_EFFECT(r_startup);
}

ENTRY_POINT(StartupTest, FOREVER, false);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}