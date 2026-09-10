#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

static int tag_final_calls = 0;
static int shutdown_calls = 0;
static bool saw_shutdown_tag = false;

static void count_on_tag_complete(void* state, const LfExtensionTagContext* context) {
  (void)state;
  tag_final_calls++;
  saw_shutdown_tag = saw_shutdown_tag || context->is_shutdown;
}
static void count_on_shutdown(void* state, Environment* environment) {
  (void)state;
  (void)environment;
  shutdown_calls++;
}

static const LfExtensionDescriptor descriptor = {.api_version = LF_RUNTIME_EXTENSION_API_VERSION,
                                                 .struct_size = sizeof(LfExtensionDescriptor),
                                                 .required_capabilities =
                                                     LF_EXTENSION_CAP_TAG_COMPLETE | LF_EXTENSION_CAP_SHUTDOWN,
                                                 .name = "test.shutdown-path",
                                                 .on_tag_complete = count_on_tag_complete,
                                                 .on_shutdown = count_on_shutdown};
static LfExtension ext = LF_EXTENSION_INSTANCE_INIT(&descriptor, NULL);

LF_DEFINE_ACTION_STRUCT_VOID(StarveTest, a, LogicalAction, 1, 1, 0, 2)
LF_DEFINE_ACTION_CTOR_VOID(StarveTest, a, LogicalAction, ACTION_POLICY_DEFER, 1, 1, 0, 2)
LF_DEFINE_STARTUP_STRUCT(StarveTest, 1, 0)
LF_DEFINE_STARTUP_CTOR(StarveTest)
LF_DEFINE_REACTION_STRUCT(StarveTest, r_startup, 1)
LF_DEFINE_REACTION_CTOR(StarveTest, r_startup, 0, NULL, NULL)
LF_DEFINE_REACTION_STRUCT(StarveTest, r_act, 0)
LF_DEFINE_REACTION_CTOR(StarveTest, r_act, 1, NULL, NULL)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(StarveTest, r_startup);
  LF_REACTION_INSTANCE(StarveTest, r_act);
  LF_STARTUP_INSTANCE(StarveTest);
  LF_ACTION_INSTANCE(StarveTest, a);
  LF_REACTOR_BOOKKEEPING_INSTANCES(2, 2, 0);
} StarveTest;

LF_DEFINE_REACTION_BODY(StarveTest, r_startup) {
  LF_SCOPE_SELF(StarveTest);
  LF_SCOPE_ACTION(StarveTest, a);
  lf_schedule(a, 0);
}

LF_DEFINE_REACTION_BODY(StarveTest, r_act) { (void)_self; }

LF_REACTOR_CTOR_SIGNATURE(StarveTest) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(StarveTest);
  LF_INITIALIZE_REACTION(StarveTest, r_startup, NEVER);
  LF_INITIALIZE_REACTION(StarveTest, r_act, NEVER);
  LF_INITIALIZE_STARTUP(StarveTest);
  LF_INITIALIZE_ACTION(StarveTest, a, MSEC(0), MSEC(0));
  LF_STARTUP_REGISTER_EFFECT(self->r_startup);
  LF_ACTION_REGISTER_EFFECT(self->a, self->r_act);
  LF_ACTION_REGISTER_SOURCE(self->a, self->r_startup);
  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(env, &ext));
}

LF_ENTRY_POINT(StarveTest, 32, 32, SEC(1), false, false)

void test_shutdown_tag_is_explicitly_identified(void) {
  TEST_ASSERT_EQUAL_MESSAGE(3, tag_final_calls, "every distinct completed tag must be reported");
  TEST_ASSERT_TRUE(saw_shutdown_tag);
  TEST_ASSERT_EQUAL(1, shutdown_calls);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  RUN_TEST(test_shutdown_tag_is_explicitly_identified);
  return UNITY_END();
}
