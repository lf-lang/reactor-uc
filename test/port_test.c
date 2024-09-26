#include "reactor-uc/reactor-uc.h"
#include "unity.h"

// Reactor Sender
typedef struct {
  Timer super;
  Reaction *effects[1];
} Timer1;

typedef struct {
  Reaction super;
} Reaction1;

typedef struct {
  OutputPort super;
  Reaction *sources[1];
  int value;
} Out;

struct Sender {
  Reactor super;
  Reaction1 reaction;
  Timer1 timer;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
};

void timer_handler(Reaction *_self) {
  struct Sender *self = (struct Sender *)_self->parent;
  Environment *env = self->super.env;
  Out *out = &self->out;

  printf("Timer triggered @ %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, 42);
}

void Reaction1_ctor(Reaction1 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, timer_handler, NULL, 0, 0);
}

void Out_ctor(Out *self, struct Sender *parent) {
  self->sources[0] = &parent->reaction.super;
  OutputPort_ctor(&self->super, &parent->super, self->sources, 1);
}

void Sender_ctor(struct Sender *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "Sender", env, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction1_ctor(&self->reaction, &self->super);
  Timer_ctor(&self->timer.super, &self->super, 0, SEC(1), self->timer.effects, 1);
  Out_ctor(&self->out, self);
  self->timer.super.super.register_effect(&self->timer.super.super, &self->reaction.super);

  // Register reaction as a source for out
  ((Trigger *)&self->out)->register_source((Trigger *)&self->out, &self->reaction.super);
}

// Reactor Receiver
typedef struct {
  Reaction super;
} Reaction2;

typedef struct {
  InputPort super;
  int buffer[1];
  Reaction *effects[1];
} In;

struct Receiver {
  Reactor super;
  Reaction2 reaction;
  In inp;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
};

void In_ctor(In *self, struct Receiver *parent) {
  InputPort_ctor(&self->super, &parent->super, self->effects, 1, sizeof(self->buffer[0]), self->buffer, 1);
}

void input_handler(Reaction *_self) {
  struct Receiver *self = (struct Receiver *)_self->parent;
  Environment *env = self->super.env;
  In *inp = &self->inp;

  printf("Input triggered @ %ld with %d\n", env->get_elapsed_logical_time(env), lf_get(inp));
}

void Reaction2_ctor(Reaction2 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, input_handler, NULL, 0, 0);
}

void Receiver_ctor(struct Receiver *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->inp;
  Reactor_ctor(&self->super, "Receiver", env, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction2_ctor(&self->reaction, &self->super);
  In_ctor(&self->inp, self);

  // Register reaction as an effect of in
  ((Trigger *)&self->inp)->register_effect((Trigger *)&self->inp, &self->reaction.super);
}

struct Conn1 {
  Connection super;
  InputPort *downstreams[1];
};

void Conn1_ctor(struct Conn1 *self, Reactor *parent, OutputPort *upstream) {
  Connection_ctor(&self->super, parent, &upstream->super, (Port **)self->downstreams, 1);
}

// Reactor main
struct Main {
  Reactor super;
  struct Sender sender;
  struct Receiver receiver;
  struct Conn1 conn;

  Reactor *_children[2];
};

void Main_ctor(struct Main *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, env);

  self->_children[1] = &self->receiver.super;
  Receiver_ctor(&self->receiver, env);

  Conn1_ctor(&self->conn, &self->super, &self->sender.out.super);
  self->conn.super.register_downstream(&self->conn.super, &self->receiver.inp.super.super);

  Reactor_ctor(&self->super, "Main", env, self->_children, 2, NULL, 0, NULL, 0);
}

void test_simple() {
  struct Main main;
  Environment env;
  Environment_ctor(&env, (Reactor *)&main);
  Main_ctor(&main, &env);
  env.set_stop_time(&env, SEC(1));
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}