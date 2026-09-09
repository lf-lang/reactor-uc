#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

/* Registration with all fields NULL, partial, multiple; on_tag_final fires exactly
   once per tag -- including for a program with no reaction(shutdown), and under
   FEDERATED, which the `BUILD_UNIT_TESTS OR BUILD_LF_TESTS` branch of the root
   CMakeLists.txt forces ON for every test build. */

#define MAX_TAGS 16
static int full_calls = 0;
static int partial_calls = 0;
static int shutdown_calls = 0;
static tag_t seen[MAX_TAGS];

static void full_on_tag_complete(void* state, const LfExtensionTagContext* context) {
  (void)state;
  if (full_calls < MAX_TAGS) {
    seen[full_calls] = context->tag;
  }
  full_calls++;
}
static void full_on_shutdown(void* state, Environment* environment) {
  (void)state;
  (void)environment;
  shutdown_calls++;
}
static void partial_on_tag_complete(void* state, const LfExtensionTagContext* context) {
  (void)state;
  (void)context;
  partial_calls++;
}

static const LfExtensionDescriptor full_descriptor = {
    .api_version = LF_RUNTIME_EXTENSION_API_VERSION,
    .struct_size = sizeof(LfExtensionDescriptor),
    .required_capabilities = LF_EXTENSION_CAP_TAG_COMPLETE | LF_EXTENSION_CAP_SHUTDOWN,
    .name = "test.full",
    .on_tag_complete = full_on_tag_complete,
    .on_shutdown = full_on_shutdown};
static const LfExtensionDescriptor partial_descriptor = {
    .api_version = LF_RUNTIME_EXTENSION_API_VERSION,
    .struct_size = sizeof(LfExtensionDescriptor),
    .required_capabilities = LF_EXTENSION_CAP_TAG_COMPLETE,
    .name = "test.partial",
    .on_tag_complete = partial_on_tag_complete};
static const LfExtensionDescriptor empty_descriptor = {.api_version = LF_RUNTIME_EXTENSION_API_VERSION,
                                                       .struct_size = sizeof(LfExtensionDescriptor),
                                                       .required_capabilities = 0,
                                                       .name = "test.empty"};
static LfExtension ext_full = LF_EXTENSION_INSTANCE_INIT(&full_descriptor, NULL);
static LfExtension ext_partial = LF_EXTENSION_INSTANCE_INIT(&partial_descriptor, NULL);
static LfExtension ext_empty = LF_EXTENSION_INSTANCE_INIT(&empty_descriptor, NULL);

LF_DEFINE_TIMER_STRUCT(ExtTest, t, 1, 0)
LF_DEFINE_TIMER_CTOR(ExtTest, t, 1, 0)
LF_DEFINE_REACTION_STRUCT(ExtTest, r, 0)
LF_DEFINE_REACTION_CTOR(ExtTest, r, 0, NULL, NULL)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(ExtTest, r);
  LF_TIMER_INSTANCE(ExtTest, t);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
} ExtTest;

LF_DEFINE_REACTION_BODY(ExtTest, r) { (void)_self; }

LF_REACTOR_CTOR_SIGNATURE(ExtTest) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(ExtTest);
  LF_INITIALIZE_REACTION(ExtTest, r, NEVER);
  LF_INITIALIZE_TIMER(ExtTest, t, MSEC(0), MSEC(1));
  LF_TIMER_REGISTER_EFFECT(self->t, self->r);
  // Registration from a reactor constructor is the shape codegen will emit. 
  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(env, &ext_full));
  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(env, &ext_partial));
}

// 4 executed tags at 0,1,2,3 ms.
LF_ENTRY_POINT(ExtTest, 32, 32, MSEC(3), false, true)

void test_fires_once_per_tag(void) {
  TEST_ASSERT_EQUAL_MESSAGE(4, full_calls, "on_tag_final must fire exactly once per tag");
  TEST_ASSERT_EQUAL_MESSAGE(4, partial_calls, "a partially-populated extension still gets its callback");
  for (int i = 1; i < full_calls; i++) {
    TEST_ASSERT_MESSAGE(lf_tag_compare(seen[i - 1], seen[i]) < 0, "tags must be strictly increasing");
  }
}

void test_shutdown_fires_once(void) { TEST_ASSERT_EQUAL(1, shutdown_calls); }

void test_all_null_extension_is_accepted(void) {
  // Registering an extension with every field NULL must not crash the notifier. 
  Environment env;
  Environment_ctor(&env, NULL, NULL, false);
  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(&env, &ext_empty));
  Environment_notify_tag_complete(&env, FOREVER_TAG, false);
  Environment_notify_shutdown(&env);
}

// Re-registering the same extension must not add it into the list twice.
static int idem_calls = 0;
static void idem_on_tag_complete(void* state, const LfExtensionTagContext* context) {
  (void)state;
  (void)context;
  idem_calls++;
}
static const LfExtensionDescriptor idem_descriptor = {
    .api_version = LF_RUNTIME_EXTENSION_API_VERSION,
    .struct_size = sizeof(LfExtensionDescriptor),
    .required_capabilities = LF_EXTENSION_CAP_TAG_COMPLETE,
    .name = "test.idem",
    .on_tag_complete = idem_on_tag_complete};

void test_re_registering_an_extension_is_idempotent(void) {
  Environment env;
  Environment_ctor(&env, NULL, NULL, false);
  LfExtension ext = LF_EXTENSION_INSTANCE_INIT(&idem_descriptor, NULL);

  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(&env, &ext));
  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(&env, &ext));

  idem_calls = 0;
  Environment_notify_tag_complete(&env, (tag_t){.time = 1, .microstep = 0}, false);
  TEST_ASSERT_EQUAL_INT(1, idem_calls); // fires once, not twice, and terminates 
}

// Firing order is registration order.
static char order_log[4];
static size_t order_len = 0;
static void order_append(char c) {
  if (order_len < sizeof(order_log) - 1) {
    order_log[order_len++] = c;
  }
}
static void order_callback(void* state, const LfExtensionTagContext* context) {
  (void)context;
  order_append(*(const char*)state);
}
static const LfExtensionDescriptor order_descriptor = {
    .api_version = LF_RUNTIME_EXTENSION_API_VERSION,
    .struct_size = sizeof(LfExtensionDescriptor),
    .required_capabilities = LF_EXTENSION_CAP_TAG_COMPLETE,
    .name = "test.order",
    .on_tag_complete = order_callback};

void test_firing_order_is_registration_order(void) {
  Environment env;
  Environment_ctor(&env, NULL, NULL, false);
  char a = 'a';
  char b = 'b';
  char c = 'c';
  LfExtension ea = LF_EXTENSION_INSTANCE_INIT(&order_descriptor, &a);
  LfExtension eb = LF_EXTENSION_INSTANCE_INIT(&order_descriptor, &b);
  LfExtension ec = LF_EXTENSION_INSTANCE_INIT(&order_descriptor, &c);

  Environment_register_extension(&env, &ea);
  Environment_register_extension(&env, &eb);
  Environment_register_extension(&env, &ec);

  order_len = 0;
  memset(order_log, 0, sizeof(order_log));
  Environment_notify_tag_complete(&env, (tag_t){.time = 1, .microstep = 0}, false);
  TEST_ASSERT_EQUAL_STRING("abc", order_log);
}

void test_instance_cannot_cross_environments(void) {
  Environment first;
  Environment second;
  Environment_ctor(&first, NULL, NULL, false);
  Environment_ctor(&second, NULL, NULL, false);
  LfExtension extension = LF_EXTENSION_INSTANCE_INIT(&empty_descriptor, NULL);

  TEST_ASSERT_EQUAL(LF_OK, Environment_register_extension(&first, &extension));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, Environment_register_extension(&second, &extension));
}

void test_registration_is_sealed_by_first_dispatch(void) {
  Environment env;
  Environment_ctor(&env, NULL, NULL, false);
  LfExtension extension = LF_EXTENSION_INSTANCE_INIT(&empty_descriptor, NULL);

  Environment_notify_tag_complete(&env, (tag_t){.time = 1, .microstep = 0}, false);
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, Environment_register_extension(&env, &extension));
}

void test_descriptor_version_is_checked(void) {
  Environment env;
  Environment_ctor(&env, NULL, NULL, false);
  const LfExtensionDescriptor incompatible = {.api_version = LF_RUNTIME_EXTENSION_API_VERSION + 1,
                                              .struct_size = sizeof(LfExtensionDescriptor),
                                              .required_capabilities = 0,
                                              .name = "test.future"};
  LfExtension extension = LF_EXTENSION_INSTANCE_INIT(&incompatible, NULL);

  TEST_ASSERT_EQUAL(LF_INVALID_VALUE, Environment_register_extension(&env, &extension));
}

void test_trigger_bindings_are_typed_unique_and_statically_owned(void) {
  Trigger first;
  Trigger second;
  LfExtensionBinding binding;
  LfExtensionBinding duplicate;
  int state = 42;
  Trigger_ctor(&first, TRIG_ACTION, NULL, NULL, NULL, NULL);
  Trigger_ctor(&second, TRIG_ACTION, NULL, NULL, NULL, NULL);
  LfExtensionBinding_ctor(&binding);
  LfExtensionBinding_ctor(&duplicate);

  TEST_ASSERT_EQUAL(LF_OK, Trigger_bind_extension(&first, &binding, &empty_descriptor, &state));
  TEST_ASSERT_EQUAL_PTR(&state, Trigger_extension_state(&first, &empty_descriptor));
  TEST_ASSERT_EQUAL(LF_OK, Trigger_bind_extension(&first, &binding, &empty_descriptor, &state));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE,
                    Trigger_bind_extension(&first, &duplicate, &empty_descriptor, &state));
  TEST_ASSERT_EQUAL(LF_INVALID_VALUE,
                    Trigger_bind_extension(&second, &binding, &empty_descriptor, &state));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  RUN_TEST(test_fires_once_per_tag);
  RUN_TEST(test_shutdown_fires_once);
  RUN_TEST(test_all_null_extension_is_accepted);
  RUN_TEST(test_re_registering_an_extension_is_idempotent);
  RUN_TEST(test_firing_order_is_registration_order);
  RUN_TEST(test_instance_cannot_cross_environments);
  RUN_TEST(test_registration_is_sealed_by_first_dispatch);
  RUN_TEST(test_descriptor_version_is_checked);
  RUN_TEST(test_trigger_bindings_are_typed_unique_and_statically_owned);
  return UNITY_END();
}
