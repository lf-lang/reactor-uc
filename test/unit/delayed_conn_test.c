#include "reactor-uc/reactor-uc.h"
#include "unity.h"

// Components of Reactor Sender
DEFINE_TIMER_STRUCT(Timer1, 1)
DEFINE_TIMER_CTOR_FIXED(Timer1, 1, 0, MSEC(1))
DEFINE_REACTION_STRUCT(Sender, 0, 0);
DEFINE_OUTPUT_PORT_STRUCT(Out, 1, 1)
DEFINE_OUTPUT_PORT_CTOR(Out, 1, 1)

typedef struct {
  Reactor super;
  Sender_Reaction0 reaction;
  Timer1 timer;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

DEFINE_REACTION_BODY(Sender, 0) {
  Sender *self = (Sender *)_self->parent;
  Environment *env = self->super.env;
  Out *out = &self->out;

  printf("Timer triggered @ %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, env->get_elapsed_logical_time(env));
}
DEFINE_REACTION_CTOR(Sender, 0);

void Sender_ctor(Sender *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Sender_Reaction0_ctor(&self->reaction, &self->super);
  Timer1_ctor(&self->timer, &self->super);
  Out_ctor(&self->out, &self->super);

  TIMER_REGISTER_EFFECT(self->timer, self->reaction);

  // Register reaction as a source for out
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

// Reactor Receiver
DEFINE_REACTION_STRUCT(Receiver, 0, 0)
DEFINE_INPUT_PORT_STRUCT(In, 1, interval_t, 1)
DEFINE_INPUT_PORT_CTOR(In, 1, interval_t, 1)

typedef struct {
  Reactor super;
  Receiver_Reaction0 reaction;
  In inp;
  int cnt;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver;

DEFINE_REACTION_BODY(Receiver, 0) {
  Receiver *self = (Receiver *)_self->parent;
  In *inp = &self->inp;
  Environment *env = self->super.env;

  printf("Input triggered @ %ld with %ld\n", env->get_elapsed_logical_time(env), *lf_get(inp));
  TEST_ASSERT_EQUAL(*lf_get(inp) + MSEC(15), env->get_elapsed_logical_time(env));
}

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->inp;
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Receiver_Reaction0_ctor(&self->reaction, &self->super);
  In_ctor(&self->inp, &self->super);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(self->inp, self->reaction);
}

DEFINE_DELAYED_CONNECTION_STRUCT(Conn1, 1, interval_t, 2, MSEC(15))
DEFINE_DELAYED_CONNECTION_CTOR(Conn1, 1, interval_t, 2, MSEC(15), false)

// Reactor main
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
  env.scheduler.set_timeout(&env.scheduler, MSEC(100));
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}