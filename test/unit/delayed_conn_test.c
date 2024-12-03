#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#include <reactor-uc/schedulers/dynamic/scheduler.h>

// Components of Reactor Sender
LF_DEFINE_TIMER_STRUCT(Sender, t, 1, 0);
LF_DEFINE_TIMER_CTOR(Sender, t, 1, 0);
LF_DEFINE_REACTION_STRUCT(Sender, r_sender, 1);
LF_DEFINE_REACTION_CTOR(Sender, r_sender, 0);
LF_DEFINE_OUTPUT_STRUCT(Sender, out, 1, interval_t);
LF_DEFINE_OUTPUT_CTOR(Sender, out, 1);

typedef struct {
  Reactor super;
  RELF_ACTION_INSTANCE(Sender, r_sender);
  LF_TIMER_INSTANCE(Sender, t);
  LF_PORT_INSTANCE(Sender, out, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} Sender;

LF_DEFINE_REACTION_BODY(Sender, r_sender) {
  LF_SCOPE_SELF(Sender);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Sender, out);
  // printf("Timer triggered @ %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, env->get_elapsed_logical_time(env));
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs *out_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Sender);
  LF_INITIALIZE_REACTION(Sender, r_sender);
  LF_INITIALIZE_TIMER(Sender, t, MSEC(0), MSEC(10));
  LF_INITIALIZE_OUTPUT(Sender, out, 1, out_external);

  LF_TIMER_REGISTER_EFFECT(self->t, self->r_sender);
  LF_PORT_REGISTER_SOURCE(self->out, self->r_sender, 1);
}

// Reactor Receiver

LF_DEFINE_REACTION_STRUCT(Receiver, r_recv, 0)
LF_DEFINE_REACTION_CTOR(Receiver, r_recv, 0)
LF_DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, instant_t, 0)
LF_DEFINE_INPUT_CTOR(Receiver, in, 1, 0, instant_t, 0)

typedef struct {
  Reactor super;
  RELF_ACTION_INSTANCE(Receiver, r_recv);
  LF_PORT_INSTANCE(Receiver, in, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} Receiver;

LF_DEFINE_REACTION_BODY(Receiver, r_recv) {
  LF_SCOPE_SELF(Receiver);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Receiver, in);

  printf("Input triggered @ %ld with %ld\n", env->get_elapsed_logical_time(env), in->value);
  TEST_ASSERT_EQUAL(in->value + MSEC(15), env->get_elapsed_logical_time(env));
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *sources_in) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Receiver);
  LF_INITIALIZE_REACTION(Receiver, r_recv);
  LF_INITIALIZE_INPUT(Receiver, in, 1, sources_in);

  // Register reaction as an effect of in
  LF_PORT_REGISTER_EFFECT(self->in, self->r_recv, 1);
}

// Reactor main
LF_DEFINE_DELAYED_CONNECTION_STRUCT(Main, sender_out, 1, interval_t, 2, MSEC(15))
LF_DEFINE_DELAYED_CONNECTION_CTOR(Main, sender_out, 1, interval_t, 2, MSEC(15), false)

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Sender, sender, 1);
  LF_CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  LF_DELAYED_CONNECTION_INSTANCE(Main, sender_out, 1, 1);

  LF_CHILD_OUTPUT_CONNECTIONS(sender, out,1,1, 1);
  LF_CHILD_OUTPUT_EFFECTS(sender, out,1,1, 0);
  LF_CHILD_OUTPUT_OBSERVERS(sender, out,1,1, 0);
  LF_CHILD_INPUT_SOURCES(receiver, in,1,1, 0);
  LF_REACTOR_BOOKKEEPING_INSTANCES(0,0,2);
} Main;

LF_REACTOR_CTOR_SIGNATURE(Main) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Main);

  LF_DEFINE_CHILD_OUTPUT_ARGS(sender, out, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, 1, _sender_out_args);
  LF_DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, _receiver_in_args);

  LF_INITIALIZE_DELAYED_CONNECTION(Main, sender_out, 1, 1);
  LF_CONN_REGISTER_UPSTREAM(sender_out, self->sender, out, 1, 1);
  LF_CONN_REGISTER_DOWNSTREAM(sender_out, 1,1, self->receiver, in, 1, 1);
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