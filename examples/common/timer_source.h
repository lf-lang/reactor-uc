#include "reactor-uc/reactor-uc.h"

DEFINE_TIMER_STRUCT(TimerSource, t, 1);
DEFINE_TIMER_CTOR(TimerSource, t, 1);
DEFINE_REACTION_STRUCT(TimerSource, r, 1);
DEFINE_REACTION_CTOR(TimerSource, r, 0);

typedef struct {
  Reactor super;
  TIMER_INSTANCE(TimerSource, t);
  REACTION_INSTANCE(TimerSource, r);
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} TimerSource;


void TimerSource_ctor(TimerSource *self, Environment *env) {
  Reactor_ctor(&self->super, "TimerSource", env, NULL, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  INITIALIZE_REACTION(TimerSource, r);
  INITIALIZE_TIMER(TimerSource, t, MSEC(0), MSEC(500));
  TIMER_REGISTER_EFFECT(t, r);
}

ENTRY_POINT(TimerSource, SEC(1), false);