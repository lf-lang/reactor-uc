#include "board.h"
#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>

DEFINE_TIMER_STRUCT(Blinky, t, 1);
DEFINE_TIMER_CTOR(Blinky, t, 1);
DEFINE_REACTION_STRUCT(Blinky, r, 1);
DEFINE_REACTION_CTOR(Blinky, r, 0);

typedef struct {
  Reactor super;
  TIMER_INSTANCE(Blinky, t);
  REACTION_INSTANCE(Blinky, r);
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Blinky;

DEFINE_REACTION_BODY(Blinky, r) {
  SCOPE_SELF(Blinky);
  SCOPE_ENV();
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
  LED0_TOGGLE;
}

void Blinky_ctor(Blinky *self, Environment *env) {
  Reactor_ctor(&self->super, "Blinky", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_REACTION(Blinky, r);
  INITIALIZE_TIMER(Blinky, t, MSEC(0), MSEC(500));
  TIMER_REGISTER_EFFECT(t, r);
}

ENTRY_POINT(Blinky, FOREVER, false);
int main() {
  lf_start();
}
