#include "reactor-uc/reactor-uc.h"

LF_DEFINE_TIMER_STRUCT(TimerSource, t, 1, 0);
LF_DEFINE_TIMER_CTOR(TimerSource, t, 1, 0);
LF_DEFINE_REACTION_STRUCT(TimerSource, r, 0);
LF_DEFINE_REACTION_CTOR(TimerSource, r, 0);

typedef struct {
  Reactor super;
  LF_TIMER_INSTANCE(TimerSource, t);
  LF_REACTION_INSTANCE(TimerSource, r);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} TimerSource;


LF_REACTOR_CTOR_SIGNATURE(TimerSource) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(TimerSource);
  LF_INITIALIZE_REACTION(TimerSource, r);
  LF_INITIALIZE_TIMER(TimerSource, t, MSEC(0), MSEC(500));
  LF_TIMER_REGISTER_EFFECT(self->t, self->r);
}

LF_ENTRY_POINT(TimerSource, SEC(1), false, false);