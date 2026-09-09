#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

static int seq_counter;
static int seq_at_tag_start;
static int seq_at_reaction;
static int seq_at_tag_complete;

static int shutdown_seq_at_tag_start;
static int shutdown_seq_at_reaction;
static int shutdown_seq_at_tag_complete;

static void probe_on_tag_start(void* state, const LfExtensionTagContext* context) {
  (void)state;
  if (context->is_shutdown) {
    shutdown_seq_at_tag_start = ++seq_counter;
  } else {
    seq_at_tag_start = ++seq_counter;
  }
}

static void probe_on_tag_complete(void* state, const LfExtensionTagContext* context) {
  (void)state;
  if (context->is_shutdown) {
    shutdown_seq_at_tag_complete = ++seq_counter;
  } else {
    seq_at_tag_complete = ++seq_counter;
  }
}

static const LfExtensionDescriptor descriptor = {
    .api_version = LF_RUNTIME_EXTENSION_API_VERSION,
    .struct_size = sizeof(LfExtensionDescriptor),
    .required_capabilities = LF_EXTENSION_CAP_TAG_START | LF_EXTENSION_CAP_TAG_COMPLETE,
    .name = "test.tag-start",
    .on_tag_start = probe_on_tag_start,
    .on_tag_complete = probe_on_tag_complete,
    .on_shutdown = NULL};
static LfExtension ext = LF_EXTENSION_INSTANCE_INIT(&descriptor, NULL);

LF_DEFINE_STARTUP_STRUCT(TagStartTest, 1, 0)
LF_DEFINE_STARTUP_CTOR(TagStartTest)
LF_DEFINE_SHUTDOWN_STRUCT(TagStartTest, 1, 0)
LF_DEFINE_SHUTDOWN_CTOR(TagStartTest)
LF_DEFINE_REACTION_STRUCT(TagStartTest, r_startup, 0)
LF_DEFINE_REACTION_CTOR(TagStartTest, r_startup, 0, NULL, NULL)
LF_DEFINE_REACTION_STRUCT(TagStartTest, r_shutdown, 0)
LF_DEFINE_REACTION_CTOR(TagStartTest, r_shutdown, 1, NULL, NULL)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(TagStartTest, r_startup);
  LF_REACTION_INSTANCE(TagStartTest, r_shutdown);
  LF_STARTUP_INSTANCE(TagStartTest);
  LF_SHUTDOWN_INSTANCE(TagStartTest);
  LF_REACTOR_BOOKKEEPING_INSTANCES(2, 2, 0);
} TagStartTest;

LF_DEFINE_REACTION_BODY(TagStartTest, r_startup) {
  (void)_self;
  seq_at_reaction = ++seq_counter;
}

LF_DEFINE_REACTION_BODY(TagStartTest, r_shutdown) {
  (void)_self;
  shutdown_seq_at_reaction = ++seq_counter;
}

LF_REACTOR_CTOR_SIGNATURE(TagStartTest) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(TagStartTest);
  LF_INITIALIZE_REACTION(TagStartTest, r_startup, NEVER);
  LF_INITIALIZE_REACTION(TagStartTest, r_shutdown, NEVER);
  LF_INITIALIZE_STARTUP(TagStartTest);
  LF_INITIALIZE_SHUTDOWN(TagStartTest);
  LF_STARTUP_REGISTER_EFFECT(self->r_startup);
  LF_SHUTDOWN_REGISTER_EFFECT(self->r_shutdown);
  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(env, &ext));
}

LF_ENTRY_POINT(TagStartTest, 32, 32, SEC(1), false, false)

void test_tag_start_fires_before_reactions_and_tag_complete_after(void) {
  TEST_ASSERT_TRUE_MESSAGE(seq_at_tag_start > 0, "on_tag_start must fire");
  TEST_ASSERT_TRUE_MESSAGE(seq_at_tag_start < seq_at_reaction, "on_tag_start must fire BEFORE the tag's reactions");
  TEST_ASSERT_TRUE_MESSAGE(seq_at_reaction < seq_at_tag_complete,
                            "on_tag_complete must fire AFTER the tag's reactions");

  TEST_ASSERT_TRUE_MESSAGE(shutdown_seq_at_tag_start > 0, "on_tag_start must also fire for the shutdown tag");
  TEST_ASSERT_TRUE_MESSAGE(shutdown_seq_at_tag_start < shutdown_seq_at_reaction,
                            "on_tag_start must fire BEFORE reaction(shutdown)");
  TEST_ASSERT_TRUE_MESSAGE(shutdown_seq_at_reaction < shutdown_seq_at_tag_complete,
                            "on_tag_complete must fire AFTER reaction(shutdown)");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  RUN_TEST(test_tag_start_fires_before_reactions_and_tag_complete_after);
  return UNITY_END();
}
