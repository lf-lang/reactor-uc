#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"
#include <string.h>

// Tests the flush reaction of federated output connections: a value written to the connection is sent once
// per tag, after all reactions that can write the upstream port, with the value of the last write.

#define QUEUE_SIZE 8

static Environment env;
Environment* _lf_environment = &env; // NOLINT

static instant_t mock_get_physical_time(Platform* self) {
  (void)self;
  return 0;
}
static Platform mock_platform = {.get_physical_time = mock_get_physical_time}; // used by the logging

static DynamicScheduler sched;
static EventQueue event_queue;
static ReactionQueue reaction_queue;
static ArbitraryEvent event_queue_array[QUEUE_SIZE];
static Reaction* reaction_queue_level_tail[QUEUE_SIZE];
static lf_level_word_t reaction_queue_level_occupied[LF_LEVEL_WORDS(QUEUE_SIZE)];
static Reactor parent;

// Reactions 0 and 1 write the upstream port of the federated output connection, reaction 2 does not.
static Reaction reactions[3];
static Reaction* writers[2] = {&reactions[0], &reactions[1]};
static Port upstream;

static NetworkChannel channel;
static FederatedConnectionBundle bundle;
static FederatedOutputConnection conn;
static int payload_buf;
static serialize_hook serialize_hooks[1];

// Execution log: reactions log 'a' + index, a sent message logs its value as digit.
static char exec_log[32];
static size_t log_len;

static void log_char(char c) {
  TEST_ASSERT_LESS_THAN(sizeof(exec_log) - 1, log_len);
  exec_log[log_len++] = c;
  exec_log[log_len] = '\0';
}

static int serialize_int(const void* user_struct, size_t user_struct_size, unsigned char* msg_buffer) {
  memcpy(msg_buffer, user_struct, user_struct_size); // NOLINT
  return (int)user_struct_size;
}

static bool mock_is_connected(NetworkChannel* self) {
  (void)self;
  return true;
}

static lf_ret_t mock_send_blocking(NetworkChannel* self, const FederateMessage* message) {
  (void)self;
  TEST_ASSERT_EQUAL(FederateMessage_tagged_message_tag, message->which_message);
  int value = 0;
  memcpy(&value, message->message.tagged_message.payload.bytes, sizeof(value)); // NOLINT
  log_char((char)('0' + value));
  return LF_OK;
}

static void write_value(int value) {
  conn.super.trigger_downstreams(&conn.super, sched.current_tag, &value, sizeof(value));
}

static void reaction0_body(Reaction* self) {
  (void)self;
  log_char('a');
  write_value(1);
}

static void reaction1_body(Reaction* self) {
  (void)self;
  log_char('b');
  write_value(2);
  write_value(3); // last write wins
}

static void reaction2_body(Reaction* self) {
  (void)self;
  log_char('c');
}

static void setup(void) {
  env.platform = &mock_platform;
  env.scheduler = &sched.super;
  EventQueue_ctor(&event_queue, event_queue_array, QUEUE_SIZE);
  ReactionQueue_ctor(&reaction_queue, reaction_queue_level_tail, reaction_queue_level_occupied, QUEUE_SIZE);
  DynamicScheduler_ctor(&sched, &env, &event_queue, NULL, &reaction_queue, FOREVER, false);
  sched.current_tag = ZERO_TAG;
  Reactor_ctor(&parent, "parent", &env, NULL, NULL, 0, NULL, 0, NULL, 0);

  void (*bodies[3])(Reaction*) = {reaction0_body, reaction1_body, reaction2_body};
  for (size_t i = 0; i < 3; i++) {
    Reaction_ctor(&reactions[i], &parent, bodies[i], NULL, 0, i, NULL, 0, NULL);
    reactions[i].level = (int)i;
  }
  Port_ctor(&upstream, TRIG_OUTPUT, &parent, NULL, 0, NULL, 0, writers, 2, NULL, 0, NULL, 0);
  upstream.sources.num_registered = 2;

  channel.is_connected = mock_is_connected;
  channel.send_blocking = mock_send_blocking;
  bundle.net_channel = &channel;
  serialize_hooks[0] = serialize_int;
  bundle.serialize_hooks = serialize_hooks;
  FederatedOutputConnection_ctor(&conn, &parent, &bundle, 0, &payload_buf, sizeof(payload_buf));
  conn.super.upstream = &upstream;

  log_len = 0;
  exec_log[0] = '\0';
}

static void run_tag(const Reaction* const* triggered, size_t n) {
  reaction_queue.reset(&reaction_queue);
  for (size_t i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL(LF_OK, reaction_queue.insert(&reaction_queue, (Reaction*)triggered[i]));
  }
  sched.run_timestep(&sched.super);
  TEST_ASSERT_TRUE(reaction_queue.empty(&reaction_queue));
}

/** The flush reaction has a level one higher than the highest level of the writers of the upstream port. */
void test_flush_level(void) {
  setup();
  TEST_ASSERT_EQUAL(2, conn.flush_reaction.get_level(&conn.flush_reaction));
}

/** The value of the last write is sent once, after all writers of the upstream port. */
void test_sent_once_after_last_writer(void) {
  setup();
  const Reaction* triggered[] = {&reactions[2], &reactions[1], &reactions[0]};
  run_tag(triggered, 3);
  // The flush reaction and reaction 2 share level 2. The flush was queued later, so it executes after it.
  TEST_ASSERT_EQUAL_STRING("abc3", exec_log);
}

/** The flush reaction is executed again at the next tag where the port is written. */
void test_once_per_tag(void) {
  setup();
  const Reaction* first[] = {&reactions[0]};
  run_tag(first, 1);
  const Reaction* second[] = {&reactions[2]};
  run_tag(second, 1); // no write: nothing is sent
  run_tag(first, 1);
  TEST_ASSERT_EQUAL_STRING("a1ca1", exec_log);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_flush_level);
  RUN_TEST(test_sent_once_after_last_writer);
  RUN_TEST(test_once_per_tag);
  return UNITY_END();
}
