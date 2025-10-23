#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"

LF_DEFINE_REACTION_STRUCT(TimerSource, s, 0);
LF_DEFINE_REACTION_STRUCT(TimerSource, r, 0);

LF_DEFINE_TIMER_STRUCT(TimerSource, t, 1, 0);
LF_DEFINE_STARTUP_STRUCT(TimerSource, 1, 0);

typedef struct {
  Reactor super;

  LF_TIMER_INSTANCE(TimerSource, t);
  LF_REACTION_INSTANCE(TimerSource, r);

  LF_REACTION_INSTANCE(TimerSource, s);
  LF_STARTUP_INSTANCE(TimerSource);

  LF_REACTOR_BOOKKEEPING_INSTANCES(2,2,0);
} TimerSource;

LF_DEFINE_REACTION_CTOR(TimerSource, s, 0, NULL, NULL);
LF_DEFINE_REACTION_CTOR(TimerSource, r, 1, NULL, NULL);

LF_DEFINE_STARTUP_CTOR(TimerSource);
LF_DEFINE_TIMER_CTOR(TimerSource, t, 1, 0);

LF_REACTOR_CTOR_SIGNATURE(TimerSource) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(TimerSource);
  
  LF_INITIALIZE_TIMER(TimerSource, t, MSEC(0), MSEC(500));
  LF_INITIALIZE_STARTUP(TimerSource);

  LF_INITIALIZE_REACTION(TimerSource, s, NEVER);
  LF_STARTUP_REGISTER_EFFECT(self->s);

  LF_INITIALIZE_REACTION(TimerSource, r, NEVER);
  LF_TIMER_REGISTER_EFFECT(self->t, self->r);

}

// Replace SEC(1) with FOREVER to run indefinitely. 
// You can also use MINS(...) or HOURS(...) to specify other durations.
LF_ENTRY_POINT(TimerSource,32,32, SEC(1), false, false);