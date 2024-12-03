#include "reactor-uc/reactor-uc.h"

DEFINE_TIMER_STRUCT(TimerSource, t, 1, 0);
DEFINE_TIMER_CTOR(TimerSource, t, 1, 0);
DEFINE_REACTION_STRUCT(TimerSource, r, 0);
DEFINE_REACTION_CTOR(TimerSource, r, 0);

typedef struct {
  Reactor super;
  TIMER_INSTANCE(TimerSource, t);
  REACTION_INSTANCE(TimerSource, r);
  REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} TimerSource;


REACTOR_CTOR_SIGNATURE(TimerSource) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(TimerSource);
  INITIALIZE_REACTION(TimerSource, r);
  INITIALIZE_TIMER(TimerSource, t, MSEC(0), MSEC(500));
  TIMER_REGISTER_EFFECT(self->t, self->r);
}

ENTRY_POINT(TimerSource, SEC(1), false, false);