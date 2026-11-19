#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

#include <string.h>

// Drives the scheduler tag by tag, preparing input ports with a chosen intended tag
// as late messages would, checking that STP violations are handled correctly and
// propagated downstream.

// Reactor with one reaction triggered by two inputs, with a tardy handler recording
// which inputs are late.
LF_DEFINE_REACTION_STRUCT(Sink, r, 0)
LF_DEFINE_INPUT_STRUCT(Sink, a, 1, 0, int, 0)
LF_DEFINE_INPUT_STRUCT(Sink, b, 1, 0, int, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Sink, r);
  LF_PORT_INSTANCE(Sink, a, 1);
  LF_PORT_INSTANCE(Sink, b, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 2, 0);
  int body;
  int handler;
  bool a_tardy;
  bool b_tardy;
} Sink;

LF_DEFINE_REACTION_BODY(Sink, r) {
  LF_SCOPE_SELF(Sink);
  self->body++;
}

LF_DEFINE_REACTION_STP_VIOLATION_HANDLER(Sink, r) {
  LF_SCOPE_SELF(Sink);
  LF_SCOPE_PORT(Sink, a);
  LF_SCOPE_PORT(Sink, b);
  self->handler++;
  self->a_tardy = lf_is_tardy(a);
  self->b_tardy = lf_is_tardy(b);
}

LF_DEFINE_REACTION_CTOR(Sink, r, 0, NULL, LF_REACTION_TYPE(Sink, r_stp_violation_handler))
LF_DEFINE_INPUT_CTOR(Sink, a, 1, 0, int, 0)
LF_DEFINE_INPUT_CTOR(Sink, b, 1, 0, int, 0)

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sink, InputExternalCtorArgs* a_external, InputExternalCtorArgs* b_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Sink);
  LF_INITIALIZE_INPUT(Sink, a, 1, a_external);
  LF_INITIALIZE_INPUT(Sink, b, 1, b_external);
  LF_INITIALIZE_REACTION(Sink, r, NEVER);
  LF_PORT_REGISTER_EFFECT(self->a, self->r, 1);
  LF_PORT_REGISTER_EFFECT(self->b, self->r, 1);
}

// Relay reactor, which relays its input to its output.
LF_DEFINE_REACTION_STRUCT(Relay, r, 1)
LF_DEFINE_INPUT_STRUCT(Relay, in, 1, 0, int, 0)
LF_DEFINE_OUTPUT_STRUCT(Relay, out, 1, int)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Relay, r);
  LF_PORT_INSTANCE(Relay, in, 1);
  LF_PORT_INSTANCE(Relay, out, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 2, 0);
  int body;
} Relay;

LF_DEFINE_REACTION_BODY(Relay, r) {
  LF_SCOPE_SELF(Relay);
  LF_SCOPE_PORT(Relay, in);
  LF_SCOPE_PORT(Relay, out);
  self->body++;
  lf_set(out, in->value);
}

LF_DEFINE_REACTION_CTOR(Relay, r, 0, NULL, NULL)
LF_DEFINE_INPUT_CTOR(Relay, in, 1, 0, int, 0)
LF_DEFINE_OUTPUT_CTOR(Relay, out, 1)

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Relay, OutputExternalCtorArgs* out_external,
                                          InputExternalCtorArgs* in_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Relay);
  LF_INITIALIZE_INPUT(Relay, in, 1, in_external);
  LF_INITIALIZE_OUTPUT(Relay, out, 1, out_external);
  LF_INITIALIZE_REACTION(Relay, r, NEVER);
  LF_PORT_REGISTER_EFFECT(self->in, self->r, 1);
  LF_PORT_REGISTER_SOURCE(self->out, self->r, 1);
}

// Wrapper reactor, which has a reaction with both a tardy and a deadline violation
// handler, to test that the tardy handler takes precedence.
LF_DEFINE_REACTION_STRUCT(Wrapper, r, 0)
LF_DEFINE_INPUT_STRUCT(Wrapper, in, 1, 0, int, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Wrapper, r);
  LF_PORT_INSTANCE(Wrapper, in, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
  int handler;
  int deadline_handler;
} Wrapper;

LF_DEFINE_REACTION_BODY(Wrapper, r) {}

LF_DEFINE_REACTION_STP_VIOLATION_HANDLER(Wrapper, r) {
  LF_SCOPE_SELF(Wrapper);
  self->handler++;
}

LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(Wrapper, r) {
  LF_SCOPE_SELF(Wrapper);
  self->deadline_handler++;
}

LF_DEFINE_REACTION_CTOR(Wrapper, r, 0, LF_REACTION_TYPE(Wrapper, r_deadline_violation_handler),
                        LF_REACTION_TYPE(Wrapper, r_stp_violation_handler))
LF_DEFINE_INPUT_CTOR(Wrapper, in, 1, 0, int, 0)

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Wrapper, InputExternalCtorArgs* in_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Wrapper);
  LF_INITIALIZE_INPUT(Wrapper, in, 1, in_external);
  LF_INITIALIZE_REACTION(Wrapper, r, MSEC(1));
  LF_PORT_REGISTER_EFFECT(self->in, self->r, 1);
}

// Shared reactor, which has two reactions triggered by two inputs, one of which
// writes to the output and the other.
LF_DEFINE_REACTION_STRUCT(Shared, on_time, 1)
LF_DEFINE_REACTION_STRUCT(Shared, tardy, 1)
LF_DEFINE_INPUT_STRUCT(Shared, on, 1, 0, int, 0)
LF_DEFINE_INPUT_STRUCT(Shared, late, 1, 0, int, 0)
LF_DEFINE_OUTPUT_STRUCT(Shared, out, 2, int)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Shared, on_time);
  LF_REACTION_INSTANCE(Shared, tardy);
  LF_PORT_INSTANCE(Shared, on, 1);
  LF_PORT_INSTANCE(Shared, late, 1);
  LF_PORT_INSTANCE(Shared, out, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(2, 3, 0);
  bool tardy_writes;
} Shared;

LF_DEFINE_REACTION_BODY(Shared, on_time) {
  LF_SCOPE_SELF(Shared);
  LF_SCOPE_PORT(Shared, out);
  lf_set(out, 1);
}

LF_DEFINE_REACTION_BODY(Shared, tardy) {
  LF_SCOPE_SELF(Shared);
  LF_SCOPE_PORT(Shared, out);
  if (self->tardy_writes) {
    lf_set(out, 2);
  }
}

LF_DEFINE_REACTION_CTOR(Shared, on_time, 0, NULL, NULL)
LF_DEFINE_REACTION_CTOR(Shared, tardy, 1, NULL, NULL)
LF_DEFINE_INPUT_CTOR(Shared, on, 1, 0, int, 0)
LF_DEFINE_INPUT_CTOR(Shared, late, 1, 0, int, 0)
LF_DEFINE_OUTPUT_CTOR(Shared, out, 2)

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Shared, OutputExternalCtorArgs* out_external,
                                          InputExternalCtorArgs* on_external, InputExternalCtorArgs* late_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Shared);
  LF_INITIALIZE_INPUT(Shared, on, 1, on_external);
  LF_INITIALIZE_INPUT(Shared, late, 1, late_external);
  LF_INITIALIZE_OUTPUT(Shared, out, 1, out_external);
  LF_INITIALIZE_REACTION(Shared, on_time, NEVER);
  LF_INITIALIZE_REACTION(Shared, tardy, NEVER);
  LF_PORT_REGISTER_EFFECT(self->on, self->on_time, 1);
  LF_PORT_REGISTER_EFFECT(self->late, self->tardy, 1);
  LF_PORT_REGISTER_SOURCE(self->out, self->on_time, 1);
  LF_PORT_REGISTER_SOURCE(self->out, self->tardy, 1);
}

// Main reactor, which has a reaction triggered by the output of Relay and Shared, and
// a tardy handler to record when the output of Shared is late.
LF_DEFINE_REACTION_STRUCT(Main, r, 0)
LF_DEFINE_LOGICAL_CONNECTION_STRUCT(Main, conn_relay, 1)
LF_DEFINE_LOGICAL_CONNECTION_STRUCT(Main, conn_shared, 1)
LF_DEFINE_DELAYED_CONNECTION_STRUCT(Main, conn_delayed, 1, int, 2)
LF_DEFINE_DELAYED_CONNECTION_STRUCT(Main, conn_physical, 1, int, 2)

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Relay, relay, 1);
  LF_CHILD_OUTPUT_CONNECTIONS(relay, out, 1, 1, 1);
  LF_CHILD_OUTPUT_EFFECTS(relay, out, 1, 1, 1);
  LF_CHILD_OUTPUT_OBSERVERS(relay, out, 1, 1, 0);
  LF_CHILD_INPUT_SOURCES(relay, in, 1, 1, 0);
  LF_CHILD_REACTOR_INSTANCE(Sink, sink, 1);
  LF_CHILD_INPUT_SOURCES(sink, a, 1, 1, 0);
  LF_CHILD_INPUT_SOURCES(sink, b, 1, 1, 0);
  LF_CHILD_REACTOR_INSTANCE(Wrapper, both, 1);
  LF_CHILD_INPUT_SOURCES(both, in, 1, 1, 0);
  LF_CHILD_REACTOR_INSTANCE(Shared, shared, 1);
  LF_CHILD_OUTPUT_CONNECTIONS(shared, out, 1, 1, 1);
  LF_CHILD_OUTPUT_EFFECTS(shared, out, 1, 1, 0);
  LF_CHILD_OUTPUT_OBSERVERS(shared, out, 1, 1, 0);
  LF_CHILD_INPUT_SOURCES(shared, on, 1, 1, 0);
  LF_CHILD_INPUT_SOURCES(shared, late, 1, 1, 0);
  LF_CHILD_REACTOR_INSTANCE(Sink, shared_sink, 1);
  LF_CHILD_INPUT_SOURCES(shared_sink, a, 1, 1, 0);
  LF_CHILD_INPUT_SOURCES(shared_sink, b, 1, 1, 0);
  LF_CHILD_REACTOR_INSTANCE(Sink, delay_sink, 1);
  LF_CHILD_INPUT_SOURCES(delay_sink, a, 1, 1, 0);
  LF_CHILD_INPUT_SOURCES(delay_sink, b, 1, 1, 0);
  LF_REACTION_INSTANCE(Main, r);
  LF_LOGICAL_CONNECTION_INSTANCE(Main, conn_relay, 1, 1);
  LF_LOGICAL_CONNECTION_INSTANCE(Main, conn_shared, 1, 1);
  LF_DELAYED_CONNECTION_INSTANCE(Main, conn_delayed, 1, 1);
  LF_DELAYED_CONNECTION_INSTANCE(Main, conn_physical, 1, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 0, 6);
  int body;
  int handler;
} Main;

LF_DEFINE_REACTION_BODY(Main, r) {
  LF_SCOPE_SELF(Main);
  self->body++;
}

LF_DEFINE_REACTION_STP_VIOLATION_HANDLER(Main, r) {
  LF_SCOPE_SELF(Main);
  self->handler++;
}

LF_DEFINE_REACTION_CTOR(Main, r, 0, NULL, LF_REACTION_TYPE(Main, r_stp_violation_handler))
LF_DEFINE_LOGICAL_CONNECTION_CTOR(Main, conn_relay, 1)
LF_DEFINE_LOGICAL_CONNECTION_CTOR(Main, conn_shared, 1)
LF_DEFINE_DELAYED_CONNECTION_CTOR(Main, conn_delayed, 1, 2, false)
LF_DEFINE_DELAYED_CONNECTION_CTOR(Main, conn_physical, 1, 2, true)

LF_REACTOR_CTOR_SIGNATURE(Main) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Main);
  LF_DEFINE_CHILD_OUTPUT_ARGS(relay, out, 1, 1);
  LF_DEFINE_CHILD_INPUT_ARGS(relay, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Relay, relay, 1, _relay_out_args[i], _relay_in_args[i]);
  LF_DEFINE_CHILD_INPUT_ARGS(sink, a, 1, 1);
  LF_DEFINE_CHILD_INPUT_ARGS(sink, b, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sink, sink, 1, _sink_a_args[i], _sink_b_args[i]);
  LF_DEFINE_CHILD_INPUT_ARGS(both, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Wrapper, both, 1, _both_in_args[i]);
  LF_DEFINE_CHILD_OUTPUT_ARGS(shared, out, 1, 1);
  LF_DEFINE_CHILD_INPUT_ARGS(shared, on, 1, 1);
  LF_DEFINE_CHILD_INPUT_ARGS(shared, late, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Shared, shared, 1, _shared_out_args[i], _shared_on_args[i],
                                              _shared_late_args[i]);
  LF_DEFINE_CHILD_INPUT_ARGS(shared_sink, a, 1, 1);
  LF_DEFINE_CHILD_INPUT_ARGS(shared_sink, b, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sink, shared_sink, 1, _shared_sink_a_args[i], _shared_sink_b_args[i]);
  LF_DEFINE_CHILD_INPUT_ARGS(delay_sink, a, 1, 1);
  LF_DEFINE_CHILD_INPUT_ARGS(delay_sink, b, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sink, delay_sink, 1, _delay_sink_a_args[i], _delay_sink_b_args[i]);

  LF_INITIALIZE_LOGICAL_CONNECTION(Main, conn_relay, 1, 1);
  lf_connect(&self->conn_relay[0][0].super.super, &self->relay->out[0].super, &self->sink->a[0].super);
  LF_INITIALIZE_LOGICAL_CONNECTION(Main, conn_shared, 1, 1);
  lf_connect(&self->conn_shared[0][0].super.super, &self->shared->out[0].super, &self->shared_sink->a[0].super);
  LF_INITIALIZE_DELAYED_CONNECTION(Main, conn_delayed, MSEC(1), 1, 1);
  Connection* conn_delayed = &self->conn_delayed[0][0].super.super;
  conn_delayed->register_downstream(conn_delayed, &self->delay_sink->a[0].super);
  LF_INITIALIZE_DELAYED_CONNECTION(Main, conn_physical, MSEC(1), 1, 1);
  Connection* conn_physical = &self->conn_physical[0][0].super.super;
  conn_physical->register_downstream(conn_physical, &self->delay_sink->b[0].super);

  LF_INITIALIZE_REACTION(Main, r, NEVER);
  LF_PORT_REGISTER_EFFECT(self->relay[0].out, self->r, 1);
}

static Main main_reactor;
static Environment env;
Environment* _lf_environment = &env;
static ArbitraryEvent events[8];
static EventQueue event_queue;
static Reaction* reactions[8][8];
static int level_size[8];
static ReactionQueue reaction_queue;
static DynamicScheduler scheduler;

static tag_t on_time;
static tag_t late;

static void run_tag(Port** ports, const tag_t* intended, size_t ports_size) {
  Scheduler* sched = &scheduler.super;
  sched->prepare_timestep(sched, on_time);
  for (size_t i = 0; i < ports_size; i++) {
    Event event = EVENT_INIT(on_time, &ports[i]->super, NULL);
    event.intended_tag = intended[i];
    ports[i]->super.prepare(&ports[i]->super, &event);
  }
  scheduler.run_timestep(sched);
  scheduler.clean_up_timestep(sched);
}

// Deliver an event through a connection, preparing the scheduler for the tag of the event and the intended tag of the
// event, and running the reactions triggered by the event. Returns the next event in the queue, which is expected to be
// a reaction triggered by the output of the connection.
static Event deliver_through(Connection* conn, tag_t tag, tag_t intended) {
  Scheduler* sched = &scheduler.super;
  int value = 42;
  sched->prepare_timestep(sched, tag);
  conn->trigger_downstreams(conn, intended, &value, sizeof(value));
  scheduler.run_timestep(sched);
  scheduler.clean_up_timestep(sched);

  ArbitraryEvent next;
  TEST_ASSERT_EQUAL(LF_OK, event_queue.pop(&event_queue, (AbstractEvent*)&next));
  sched->prepare_timestep(sched, next.event.super.tag);
  next.event.trigger->prepare(next.event.trigger, &next.event);
  scheduler.run_timestep(sched);
  scheduler.clean_up_timestep(sched);
  return next.event;
}

void setUp(void) {
  memset(&main_reactor, 0, sizeof(main_reactor));
  EventQueue_ctor(&event_queue, events, 8);
  ReactionQueue_ctor(&reaction_queue, (Reaction**)reactions, level_size, 8);
  DynamicScheduler_ctor(&scheduler, _lf_environment, &event_queue, NULL, &reaction_queue, FOREVER, false);
  Environment_ctor(&env, (Reactor*)&main_reactor, &scheduler.super, false);
  Main_ctor(&main_reactor, NULL, &env);
  env.assemble(&env);
  env.is_federated = true;
  on_time = (tag_t){.time = env.get_physical_time(&env) - SEC(1), .microstep = 0};
  late = (tag_t){.time = on_time.time - MSEC(5), .microstep = 0};
}

void tearDown(void) { Environment_free(&env); }

void test_tardy_and_on_time_trigger_runs_handler(void) {
  Sink* sink = main_reactor.sink;
  Port* ports[] = {&sink->a[0].super, &sink->b[0].super};
  tag_t intended[] = {on_time, late};
  run_tag(ports, intended, 2);

  TEST_ASSERT_EQUAL(0, sink->body);
  TEST_ASSERT_EQUAL(1, sink->handler);
  TEST_ASSERT_FALSE(sink->a_tardy);
  TEST_ASSERT_TRUE(sink->b_tardy);
}

void test_on_time_triggers_run_body(void) {
  Sink* sink = main_reactor.sink;
  Port* ports[] = {&sink->a[0].super, &sink->b[0].super};
  tag_t intended[] = {on_time, on_time};
  run_tag(ports, intended, 2);

  TEST_ASSERT_EQUAL(1, sink->body);
  TEST_ASSERT_EQUAL(0, sink->handler);
}

void test_violations_ignored_when_not_federated(void) {
  env.is_federated = false;
  Sink* sink = main_reactor.sink;
  Port* ports[] = {&sink->a[0].super, &sink->b[0].super};
  tag_t intended[] = {on_time, late};
  run_tag(ports, intended, 2);

  TEST_ASSERT_EQUAL(1, sink->body);
  TEST_ASSERT_EQUAL(0, sink->handler);
}

void test_unhandled_violation_is_passed_downstream(void) {
  Relay* relay = main_reactor.relay;
  Sink* sink = main_reactor.sink;
  Port* ports[] = {&relay->in[0].super};
  tag_t intended[] = {late};
  run_tag(ports, intended, 1);

  TEST_ASSERT_EQUAL(1, relay->body);
  TEST_ASSERT_EQUAL(0, sink->body);
  TEST_ASSERT_EQUAL(1, sink->handler);
  TEST_ASSERT_TRUE(sink->a_tardy);
  TEST_ASSERT_EQUAL(0, main_reactor.body);
  TEST_ASSERT_EQUAL(1, main_reactor.handler);
}

void test_on_time_value_is_not_passed_downstream(void) {
  Relay* relay = main_reactor.relay;
  Sink* sink = main_reactor.sink;
  Port* ports[] = {&relay->in[0].super};
  tag_t intended[] = {on_time};
  run_tag(ports, intended, 1);

  TEST_ASSERT_EQUAL(1, relay->body);
  TEST_ASSERT_EQUAL(1, sink->body);
  TEST_ASSERT_EQUAL(0, sink->handler);
  TEST_ASSERT_EQUAL(1, main_reactor.body);
  TEST_ASSERT_EQUAL(0, main_reactor.handler);
}

void test_tardy_handler_skips_deadline_handler(void) {
  Wrapper* both = main_reactor.both;
  Port* ports[] = {&both->in[0].super};
  tag_t intended[] = {late};
  run_tag(ports, intended, 1);

  // STP violation takes precedence over the missed deadline.
  TEST_ASSERT_EQUAL(1, both->handler);
  TEST_ASSERT_EQUAL(0, both->deadline_handler);
}

void test_unwritten_effect_is_not_passed_downstream(void) {
  Shared* shared = main_reactor.shared;
  Sink* sink = main_reactor.shared_sink;
  Port* ports[] = {&shared->on[0].super, &shared->late[0].super};
  tag_t intended[] = {on_time, late};
  run_tag(ports, intended, 2);

  TEST_ASSERT_EQUAL(1, sink->body);
  TEST_ASSERT_EQUAL(0, sink->handler);
}

void test_late_overwrite_is_passed_downstream(void) {
  Shared* shared = main_reactor.shared;
  Sink* sink = main_reactor.shared_sink;
  shared->tardy_writes = true;
  Port* ports[] = {&shared->on[0].super, &shared->late[0].super};
  tag_t intended[] = {on_time, late};
  run_tag(ports, intended, 2);

  TEST_ASSERT_EQUAL(0, sink->body);
  TEST_ASSERT_EQUAL(1, sink->handler);
  TEST_ASSERT_TRUE(sink->a_tardy);
}

void test_late_value_delayed_into_the_past_is_late_downstream(void) {
  Sink* sink = main_reactor.delay_sink;
  Event event = deliver_through(&main_reactor.conn_delayed[0][0].super.super, on_time, late);

  TEST_ASSERT_EQUAL(0, lf_tag_compare(lf_delay_tag(on_time, 0), event.super.tag));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(lf_delay_tag(late, MSEC(1)), event.intended_tag));
  TEST_ASSERT_EQUAL(0, sink->body);
  TEST_ASSERT_EQUAL(1, sink->handler);
  TEST_ASSERT_TRUE(sink->a_tardy);
}

void test_physical_connection_behind_logical_time_is_not_late(void) {
  Sink* sink = main_reactor.delay_sink;
  tag_t ahead = {.time = env.get_physical_time(&env) + SEC(10), .microstep = 0};
  Event event = deliver_through(&main_reactor.conn_physical[0][0].super.super, ahead, ahead);

  TEST_ASSERT_EQUAL(0, lf_tag_compare(lf_delay_tag(ahead, 0), event.super.tag));
  TEST_ASSERT_EQUAL(0, lf_tag_compare(event.super.tag, event.intended_tag));
  TEST_ASSERT_EQUAL(1, sink->body);
  TEST_ASSERT_EQUAL(0, sink->handler);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tardy_and_on_time_trigger_runs_handler);
  RUN_TEST(test_on_time_triggers_run_body);
  RUN_TEST(test_violations_ignored_when_not_federated);
  RUN_TEST(test_unhandled_violation_is_passed_downstream);
  RUN_TEST(test_on_time_value_is_not_passed_downstream);
  RUN_TEST(test_tardy_handler_skips_deadline_handler);
  RUN_TEST(test_unwritten_effect_is_not_passed_downstream);
  RUN_TEST(test_late_overwrite_is_passed_downstream);
  RUN_TEST(test_late_value_delayed_into_the_past_is_late_downstream);
  RUN_TEST(test_physical_connection_behind_logical_time_is_not_late);
  return UNITY_END();
}
