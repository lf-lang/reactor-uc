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
  REACTOR_BOOKKEEPING_INSTANCES(2,2,0);
  int cnt;
} ShutdownTest;

DEFINE_REACTION_BODY(ShutdownTest, r_startup) {
  SCOPE_SELF(ShutdownTest);
  TEST_ASSERT_EQUAL(0, self->cnt);
  self->cnt = 42;
}

DEFINE_REACTION_BODY(ShutdownTest, r_shutdown) {
  SCOPE_SELF(ShutdownTest);
  TEST_ASSERT_EQUAL(42, self->cnt);
  self->cnt = 67;
}

REACTOR_CTOR_SIGNATURE(ShutdownTest) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(ShutdownTest);
  INITIALIZE_STARTUP(ShutdownTest);
  INITIALIZE_REACTION(ShutdownTest, r_startup);
  STARTUP_REGISTER_EFFECT(r_startup);

  INITIALIZE_SHUTDOWN(ShutdownTest);
  INITIALIZE_REACTION(ShutdownTest, r_shutdown);
  SHUTDOWN_REGISTER_EFFECT(r_shutdown);
}

ENTRY_POINT(ShutdownTest, FOREVER, false);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}