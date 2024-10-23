#include "board.h"
#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>

/* Structs */
DEFINE_TIMER_STRUCT(MyTimer, 1)
DEFINE_REACTION_STRUCT(MyReactor, 0, 0)

typedef struct {
  Reactor super;
  MyReactor_Reaction0 my_reaction;
  MyTimer timer;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} MyReactor;

/* Reaction Bodies */
DEFINE_REACTION_BODY(MyReactor, 0) {
  MyReactor *self = (MyReactor *)_self->parent;
  Environment *env = self->super.env;
  LED0_TOGGLE;
  printf("Hello World @ %" PRId64 "\n", env->get_elapsed_physical_time(env));
}

/* Constructors */
DEFINE_TIMER_CTOR_FIXED(MyTimer, 1, MSEC(0), MSEC(100))
DEFINE_REACTION_CTOR(MyReactor, 0)

void MyReactor_ctor(MyReactor *self, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->my_reaction;
  self->_triggers[0] = (Trigger *)&self->timer;

  Reactor_ctor(&self->super, "MyReactor", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  MyReactor_Reaction0_ctor(&self->my_reaction, &self->super);
  MyTimer_ctor(&self->timer, &self->super);

  TIMER_REGISTER_EFFECT(self->timer, self->my_reaction);
}

/* Application */
ENTRY_POINT(MyReactor, FOREVER, true)

int main(void) {
  lf_start();
  return 0;
}
