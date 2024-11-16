#include "reactor-uc/reactor-uc.h"

#include "unity.h"

DEFINE_STARTUP_STRUCT(StartupTest, 1, 0)
DEFINE_STARTUP_CTOR(StartupTest)
DEFINE_REACTION_STRUCT(StartupTest, r_startup, 0)
DEFINE_REACTION_CTOR(StartupTest, r_startup, 0)

typedef struct {
  Reactor super;
  REACTION_INSTANCE(StartupTest, r_startup);
  STARTUP_INSTANCE(StartupTest);  
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
  int cnt;
} StartupTest;

DEFINE_REACTION_BODY(StartupTest, r_startup) { printf("Hello World\n"); }

REACTOR_CTOR_SIGNATURE(StartupTest) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(StartupTest);
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