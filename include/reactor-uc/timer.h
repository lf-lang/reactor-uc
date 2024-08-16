
#ifndef REACTOR_UC_TIMER_H
#define REACTOR_UC_TIMER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Timer Timer;

struct Timer {
  Trigger super;
  instant_t offset;
  interval_t period;
} __attribute__ ((aligned (32)));

void Timer_ctor(Timer *self, Reactor *parent, instant_t offset, interval_t period, Reaction **effects,
                size_t effects_size);

#endif
