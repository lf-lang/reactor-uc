#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>

DEFINE_TIMER_STRUCT(Hello, t, 1);
DEFINE_TIMER_CTOR(Hello, t, 1);
DEFINE_REACTION_STRUCT(Hello, r, 1);
DEFINE_REACTION_CTOR(Hello, r, 0);

typedef struct {
  Reactor super;
  TIMER_INSTANCE(Hello, t);
  REACTION_INSTANCE(Hello, r);
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Hello;

DEFINE_REACTION_BODY(Hello, r) {
  SCOPE_SELF(Hello);
  SCOPE_ENV();
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
}

void Hello_ctor(Hello *self, Environment *env) {
  Reactor_ctor(&self->super, "Hello", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_REACTION(Hello, r);
  INITIALIZE_TIMER(Hello, t, MSEC(0), MSEC(500));
  TIMER_REGISTER_EFFECT(t, r);
}

ENTRY_POINT(Hello, FOREVER, false);
int main() {
  lf_start();
}
