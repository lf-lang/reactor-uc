#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#include <reactor-uc/schedulers/dynamic/scheduler.h>

// Components of Reactor Sender
DEFINE_TIMER_STRUCT(Sender, t, 1, 0);
DEFINE_TIMER_CTOR(Sender, t, 1, 0);
DEFINE_REACTION_STRUCT(Sender, r_sender, 1);
DEFINE_REACTION_CTOR(Sender, r_sender, 0);
DEFINE_OUTPUT_STRUCT(Sender, out, 1, interval_t);
DEFINE_OUTPUT_CTOR(Sender, out, 1);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Sender, r_sender);
  TIMER_INSTANCE(Sender, t);
  PORT_INSTANCE(Sender, out, 1);
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} Sender;

DEFINE_REACTION_BODY(Sender, r_sender) {
  SCOPE_SELF(Sender);
  SCOPE_ENV();
  SCOPE_PORT(Sender, out);
  // printf("Timer triggered @ %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, env->get_elapsed_logical_time(env));
}

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs *out_external) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Sender);
  INITIALIZE_REACTION(Sender, r_sender);
  INITIALIZE_TIMER(Sender, t, MSEC(0), MSEC(10));
  INITIALIZE_OUTPUT(Sender, out, 1, out_external);

  TIMER_REGISTER_EFFECT(self->t, self->r_sender);
  PORT_REGISTER_SOURCE(self->out, self->r_sender, 1);
}

// Reactor Receiver

DEFINE_REACTION_STRUCT(Receiver, r_recv, 0)
DEFINE_REACTION_CTOR(Receiver, r_recv, 0)
DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, instant_t, 0)
DEFINE_INPUT_CTOR(Receiver, in, 1, 0, instant_t, 0)

typedef struct {
  Reactor super;
  REACTION_INSTANCE(Receiver, r_recv);
  PORT_INSTANCE(Receiver, in, 1);
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} Receiver;

DEFINE_REACTION_BODY(Receiver, r_recv) {
  SCOPE_SELF(Receiver);
  SCOPE_ENV();
  SCOPE_PORT(Receiver, in);

  printf("Input triggered @ %ld with %ld\n", env->get_elapsed_logical_time(env), in->value);
  TEST_ASSERT_EQUAL(in->value + MSEC(15), env->get_elapsed_logical_time(env));
}

REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *sources_in) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Receiver);
  INITIALIZE_REACTION(Receiver, r_recv);
  INITIALIZE_INPUT(Receiver, in, 1, sources_in);

  // Register reaction as an effect of in
  PORT_REGISTER_EFFECT(self->in, self->r_recv, 1);
}

// Reactor main
DEFINE_DELAYED_CONNECTION_STRUCT(Main, sender_out, 1, interval_t, 2, MSEC(15))
DEFINE_DELAYED_CONNECTION_CTOR(Main, sender_out, 1, interval_t, 2, MSEC(15), false)

typedef struct {
  Reactor super;
  CHILD_REACTOR_INSTANCE(Sender, sender, 1);
  CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  DELAYED_CONNECTION_INSTANCE(Main, sender_out, 1, 1);

  CHILD_OUTPUT_CONNECTIONS(sender, out,1,1, 1);
  CHILD_OUTPUT_EFFECTS(sender, out,1,1, 0);
  CHILD_OUTPUT_OBSERVERS(sender, out,1,1, 0);
  CHILD_INPUT_SOURCES(receiver, in,1,1, 0);
  REACTOR_BOOKKEEPING_INSTANCES(0,0,2);
} Main;

REACTOR_CTOR_SIGNATURE(Main) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(Main);

  DEFINE_CHILD_OUTPUT_ARGS(sender, out, 1, 1);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, 1, _sender_out_args);
  DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, _receiver_in_args);

  INITIALIZE_DELAYED_CONNECTION(Main, sender_out, 1, 1);
  CONN_REGISTER_UPSTREAM(sender_out, self->sender, out, 1, 1);
  CONN_REGISTER_DOWNSTREAM(sender_out, 1,1, self->receiver, in, 1, 1);
}

void test_simple() {
  Main main;
  Environment env;
  Environment_ctor(&env, (Reactor *)&main);
  Main_ctor(&main, NULL, &env);
  env.scheduler->duration = MSEC(100);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}