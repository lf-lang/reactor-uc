#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"

LF_DEFINE_TIMER_STRUCT(TimerSource, t, 1, 0);
LF_DEFINE_TIMER_CTOR(TimerSource, t, 1, 0);
LF_DEFINE_REACTION_STRUCT(TimerSource, r, 0);
LF_DEFINE_REACTION_CTOR(TimerSource, r, 0, NULL, NULL);

typedef struct {
  Reactor super;
  LF_TIMER_INSTANCE(TimerSource, t);
  LF_REACTION_INSTANCE(TimerSource, r);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
} TimerSource;


LF_REACTOR_CTOR_SIGNATURE(TimerSource) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(TimerSource);
  LF_INITIALIZE_REACTION(TimerSource, r, NEVER);
  LF_INITIALIZE_TIMER(TimerSource, t, MSEC(0), MSEC(500));
  LF_TIMER_REGISTER_EFFECT(self->t, self->r);
}

#ifdef TIMEOUT 
LF_ENTRY_POINT(TimerSource,32,32, SEC(TIMEOUT), false, false);
#elif defined(TIMEOUT_FOREVER)
LF_ENTRY_POINT(TimerSource,32,32, FOREVER, false, false);
#else
LF_ENTRY_POINT(TimerSource,32,32, SEC(1), false, false);
#endif