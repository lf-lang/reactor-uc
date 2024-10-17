#include "reactor-uc/reactor-uc.h"
#include "unity.h"

// Components of Reactor Sender
DEFINE_TIMER(Timer1, 1);
DEFINE_REACTION(Reaction1, 0);
DEFINE_OUTPUT_PORT(Out, 1);

typedef struct {
  Reactor super;
  Reaction1 reaction;
  Timer1 timer;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

CONSTRUCT_REACTION(Reaction1, Sender, 0, {
  Out *out = &self->out;

  printf("Timer triggered @ %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, env->get_elapsed_logical_time(env));
});

CONSTRUCT_OUTPUT_PORT(Out, Sender)

void Sender_ctor(Sender *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction1_ctor(&self->reaction, self);
  Timer_ctor(&self->timer.super, &self->super, 0, SEC(1), self->timer.effects, 1);
  Out_ctor(&self->out, self);
  TIMER_REGISTER_EFFECT(self->timer, self->reaction);
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

// Reactor Receiver

DEFINE_REACTION(Reaction2, 0);
DEFINE_INPUT_PORT(In, 1, instant_t, 1);

typedef struct {
  Reactor super;
  Reaction2 reaction;
  In inp;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver ;

CONSTRUCT_INPUT_PORT(In, Receiver);

CONSTRUCT_REACTION(Reaction2, Receiver, 0, {
  In *inp = &self->inp;

  printf("Input triggered @ %ld with %ld\n", env->get_elapsed_logical_time(env), lf_get(inp));
  TEST_ASSERT_EQUAL(lf_get(inp), env->get_elapsed_logical_time(env));
});

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->inp;
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction2_ctor(&self->reaction, self);
  In_ctor(&self->inp, self);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(self->inp, self->reaction);
}

struct Conn1 {
  LogicalConnection super;
  Input *downstreams[1];
};

void Conn1_ctor(struct Conn1 *self, Reactor *parent) {
  LogicalConnection_ctor(&self->super, parent, (Port **)self->downstreams, 1);
}

// Reactor main
struct Main {
  Reactor super;
  Sender sender;
  Receiver receiver;
  struct Conn1 conn;

  Reactor *_children[2];
};

void Main_ctor(struct Main *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env);

  self->_children[1] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  Conn1_ctor(&self->conn, &self->super);
  CONN_REGISTER_UPSTREAM(self->conn, self->sender.out);
  CONN_REGISTER_DOWNSTREAM(self->conn, self->receiver.inp);

  Reactor_ctor(&self->super, "Main", env, NULL, self->_children, 2, NULL, 0, NULL, 0);
}

void test_simple() {
  struct Main main;
  Environment env;
  Environment_ctor(&env, (Reactor *)&main);
  Main_ctor(&main, &env);
  env.scheduler.set_timeout(&env.scheduler, SEC(1));
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}