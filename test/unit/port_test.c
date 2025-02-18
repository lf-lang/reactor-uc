#include "reactor-uc/reactor-uc.h"
#include "unity.h"

#include <reactor-uc/schedulers/dynamic/scheduler.h>

// Components of Reactor Sender
LF_DEFINE_TIMER_STRUCT(Sender, t, 1, 0);
LF_DEFINE_TIMER_CTOR(Sender, t, 1, 0);
LF_DEFINE_REACTION_STRUCT(Sender, r_sender, 1);
LF_DEFINE_REACTION_CTOR(Sender, r_sender, 0, NULL, NEVER, NULL);
LF_DEFINE_OUTPUT_STRUCT(Sender, out, 1, interval_t);
LF_DEFINE_OUTPUT_CTOR(Sender, out, 1);

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Sender, r_sender);
  LF_TIMER_INSTANCE(Sender, t);
  LF_PORT_INSTANCE(Sender, out, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
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
  LF_INITIALIZE_TIMER(Sender, t, MSEC(0), MSEC(5));
  LF_INITIALIZE_OUTPUT(Sender, out, 1, out_external);

  LF_TIMER_REGISTER_EFFECT(self->t, self->r_sender);
  LF_PORT_REGISTER_SOURCE(self->out, self->r_sender, 1);
}

// Reactor Receiver

LF_DEFINE_REACTION_STRUCT(Receiver, r_recv, 0)
LF_DEFINE_REACTION_CTOR(Receiver, r_recv, 0, NULL, NEVER, NULL)
LF_DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, instant_t, 0)
LF_DEFINE_INPUT_CTOR(Receiver, in, 1, 0, instant_t, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Receiver, r_recv);
  LF_PORT_INSTANCE(Receiver, in, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0)
} Receiver;

LF_DEFINE_REACTION_BODY(Receiver, r_recv) {
  LF_SCOPE_SELF(Receiver);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Receiver, in);

  printf("Input triggered @ %ld with %ld\n", env->get_elapsed_logical_time(env), in->value);
  TEST_ASSERT_EQUAL(in->value, env->get_elapsed_logical_time(env));
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  LF_REACTOR_CTOR(Receiver);
  LF_REACTOR_CTOR_PREAMBLE();
  LF_INITIALIZE_REACTION(Receiver, r_recv);
  LF_INITIALIZE_INPUT(Receiver, in, 1, in_external);

  // Register reaction as an effect of in
  LF_PORT_REGISTER_EFFECT(self->in, self->r_recv, 1);
}

// Reactor main
LF_DEFINE_LOGICAL_CONNECTION_STRUCT(Main, sender_out, 1)
LF_DEFINE_LOGICAL_CONNECTION_CTOR(Main, sender_out, 1)

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Sender, sender, 1);
  LF_CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  LF_LOGICAL_CONNECTION_INSTANCE(Main, sender_out, 1, 1);
  LF_REACTOR_BOOKKEEPING_INSTANCES(0, 0, 2)
  LF_CHILD_OUTPUT_CONNECTIONS(sender, out, 1, 1, 1);
  LF_CHILD_OUTPUT_EFFECTS(sender, out, 1, 1, 0);
  LF_CHILD_OUTPUT_OBSERVERS(sender, out, 1, 1, 0);
  LF_CHILD_INPUT_SOURCES(receiver, in, 1, 1, 0);
} Main;

LF_REACTOR_CTOR_SIGNATURE(Main) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Main);

  LF_DEFINE_CHILD_OUTPUT_ARGS(sender, out, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, 1, &_sender_out_args[0][0]);
  LF_DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, &_receiver_in_args[0][0]);

  LF_INITIALIZE_LOGICAL_CONNECTION(Main, sender_out, 1, 1);
  lf_connect(&self->sender_out[0][0].super.super, &self->sender->out[0].super, &self->receiver->in[0].super);
}

LF_ENTRY_POINT(Main, 32, 32, MSEC(100), false, false);

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  return UNITY_END();
}