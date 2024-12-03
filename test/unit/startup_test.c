#include "reactor-uc/reactor-uc.h"
#include "unity.h"

LF_DEFINE_STARTUP_STRUCT(StartupTest, 1, 0)
LF_DEFINE_STARTUP_CTOR(StartupTest)
LF_DEFINE_REACTION_STRUCT(StartupTest, r_startup, 0)
LF_DEFINE_REACTION_CTOR(StartupTest, r_startup, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(StartupTest, r_startup);
  LF_STARTUP_INSTANCE(StartupTest);  
  LF_REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
  int cnt;
} StartupTest;

LF_DEFINE_REACTION_BODY(StartupTest, r_startup) { printf("Hello World\n"); }

LF_REACTOR_CTOR_SIGNATURE(StartupTest) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(StartupTest);
  LF_INITIALIZE_STARTUP(StartupTest);
  LF_INITIALIZE_REACTION(StartupTest, r_startup);
  LF_STARTUP_REGISTER_EFFECT(self->r_startup);
}

LF_ENTRY_POINT(StartupTest, FOREVER, false, false);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}