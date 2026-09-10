#include "reactor-uc/reactor-uc.h"
#include "unity.h"

typedef struct {
  LfExtension ext;
  bool reaction_on;
  int tags_seen;
} ExampleExt;

static void example_on_tag_complete(void* state, const LfExtensionTagContext* context) {
  ExampleExt* self = (ExampleExt*)state;
  (void)context;
  self->tags_seen++;
  self->reaction_on = self->tags_seen < 2;
}

static const LfExtensionDescriptor example_descriptor = {.api_version = LF_RUNTIME_EXTENSION_API_VERSION,
                                                         .struct_size = sizeof(LfExtensionDescriptor),
                                                         .required_capabilities = LF_EXTENSION_CAP_TAG_COMPLETE,
                                                         .name = "example.toggle",
                                                         .on_tag_complete = example_on_tag_complete};

/* Static: an LfExtension must outlive its registration. */
static ExampleExt example;
static Environment example_env;
Environment* _lf_environment = &example_env;

void test_an_extension_observes_every_tag_once(void) {
  Environment_ctor(&example_env, NULL, NULL, false);
  example.reaction_on = true;
  example.tags_seen = 0;
  LfExtension_ctor(&example.ext, &example_descriptor, &example);

  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(&example_env, &example.ext));

  Environment_notify_tag_complete(&example_env, (tag_t){.time = 1, .microstep = 0}, false);
  Environment_notify_tag_complete(&example_env, (tag_t){.time = 1, .microstep = 0}, false); // same tag
  Environment_notify_tag_complete(&example_env, (tag_t){.time = 2, .microstep = 0}, false);

  TEST_ASSERT_EQUAL_INT(2, example.tags_seen);
  TEST_ASSERT_FALSE(example.reaction_on);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_an_extension_observes_every_tag_once);
  return UNITY_END();
}
