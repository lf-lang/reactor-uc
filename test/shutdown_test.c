
#include "reactor-uc/reactor-uc.h"
#include "unity.h"

typedef struct {
  Startup super;
  Reaction *effects_[1];
} MyStartup;

typedef struct {
  Shutdown super;
  Reaction *effects_[1];
} MyShutdown;

typedef struct {
  Reaction super;
} Reaction1;

typedef struct {
  Reaction super;
} Reaction2;

typedef struct {
  Reactor super;
  Reaction1 reaction1;
  Reaction2 reaction2;
  MyStartup startup;
  MyShutdown shutdown;
  Reaction *_reactions[2];
  Trigger *_triggers[2];
} MyReactor;

void MyStartup_ctor(MyStartup *self, MyReactor *parent) {
  Startup_ctor(&self->super, &parent->super, self->effects_, 1);
  self->super.super.register_effect(&self->super.super, &parent->reaction1.super);
}

void Reaction1_body(Reaction *_self) {
  printf("Startup reaction executing\n");
}

void Reaction1_ctor(Reaction1 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Reaction1_body, NULL, 0, 0);
}

void MyShutdown_ctor(MyShutdown *self, MyReactor *parent) {
  Shutdown_ctor(&self->super, &parent->super, self->effects_, 1);
  self->super.super.register_effect(&self->super.super, &parent->reaction2.super);
}

void Reaction2_body(Reaction *_self) {
  printf("Shutdown reaction executing\n");
}

void Reaction2_ctor(Reaction2 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Reaction2_body, NULL, 0, 0);
}

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction1;
  self->_reactions[1] = (Reaction *)&self->reaction2;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->shutdown;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, 0, self->_reactions, 2, self->_triggers, 2);
  Reaction1_ctor(&self->reaction1, &self->super);
  Reaction2_ctor(&self->reaction2, &self->super);
  MyStartup_ctor(&self->startup, self);
  MyShutdown_ctor(&self->shutdown, self);
  self->super.register_startup(&self->super, &self->startup.super);
  self->super.register_shutdown(&self->super, &self->shutdown.super);
}

void test_simple() {
  MyReactor my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  MyReactor_ctor(&my_reactor, &env);
  env.assemble(&env);
  env.start(&env);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_simple);
  return UNITY_END();
}
