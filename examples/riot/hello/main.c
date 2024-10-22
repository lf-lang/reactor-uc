#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>

DEFINE_TIMER(MyTimer, 1, 0, MSEC(100))
DEFINE_REACTION(MyReactor, 0, 0)

typedef struct {
  Reactor super;
  MyReactor_0 my_reaction;
  MyTimer timer;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} MyReactor;

REACTION_BODY(MyReactor, 0, { printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env)); })

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReactor_0_ctor(&self->my_reaction, &self->super);
  Timer_ctor(&self->timer.super, &self->super, MSEC(0), MSEC(100), self->timer.effects, 1);
  TIMER_REGISTER_EFFECT(self->timer, self->my_reaction);
}

ENTRY_POINT(MyReactor, FOREVER, true)

int main() {
  lf_start();
  return 0;
}
