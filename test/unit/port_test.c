#include "reactor-uc/reactor-uc.h"
#include "unity.h"

// Components of Reactor Sender
DEFINE_TIMER(Timer1, 1, 0, SEC(1))
DEFINE_REACTION(Sender, 0, 0)
DEFINE_OUTPUT_PORT(Out, 1)

typedef struct {
  Reactor super;
  Sender_0 reaction;
  Timer1 timer;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

REACTION_BODY(Sender, 0, {
  Out *out = &self->out;

  printf("Timer triggered @ %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, env->get_elapsed_logical_time(env));
})

void Sender_ctor(Sender *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Sender_0_ctor(&self->reaction, &self->super);
  Timer1_ctor(&self->timer, &self->super);
  Out_ctor(&self->out, &self->super);
  TIMER_REGISTER_EFFECT(self->timer, self->reaction);
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

// Reactor Receiver

DEFINE_REACTION(Receiver, 0, 0)
DEFINE_INPUT_PORT(In, 1, instant_t, 1)

typedef struct {
  Reactor super;
  Receiver_0 reaction;
  In inp;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver ;

REACTION_BODY(Receiver, 0, {
  In *inp = &self->inp;

  printf("Input triggered @ %ld with %ld\n", env->get_elapsed_logical_time(env), lf_get(inp));
  TEST_ASSERT_EQUAL(lf_get(inp), env->get_elapsed_logical_time(env));
})

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->inp;
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Receiver_0_ctor(&self->reaction, &self->super);
  In_ctor(&self->inp, &self->super);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(self->inp, self->reaction);
}

// Reactor main
DEFINE_LOGICAL_CONNECTION(Conn1, 1)

typedef struct {
  Reactor super;
  Sender sender;
  Receiver receiver;
  Conn1 conn;

  Reactor *_children[2];
} Main;

void Main_ctor(Main *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env);

  self->_children[1] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  Conn1_ctor(&self->conn, &self->super);

  CONNECT(self->conn, self->sender.out, self->receiver.inp);

  Reactor_ctor(&self->super, "Main", env, NULL, self->_children, 2, NULL, 0, NULL, 0);
}

void test_simple() {
  Main main;
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