#ifndef REACTOR_UC_CLOCK_SLAVE_H
#define REACTOR_UC_CLOCK_SLAVE_H

#include "reactor-uc/reactor-uc.h"
#include "clock-sync.h"

DEFINE_ACTION_STRUCT(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1);
DEFINE_STARTUP_STRUCT(Startup0, 1);
DEFINE_OUTPUT_STRUCT(Out, 1, 1);
DEFINE_INPUT_STRUCT(In, 1, ClockSyncMessage, 1);

DEFINE_REACTION_STRUCT(ClockSlave, 0, 0);
DEFINE_REACTION_STRUCT(ClockSlave, 1, 1);
DEFINE_REACTION_STRUCT(ClockSlave, 2, 1);

#define CLOCK_SLAVE_NUM_REACTIONS 3
#define CLOCK_SLAVE_NUM_TRIGGERS 4

typedef struct {
  Reactor super;
  ClockSlave_Reaction0 reaction0;
  ClockSlave_Reaction1 reaction1;
  ClockSlave_Reaction2 reaction2;
  Action0 send_action;
  Startup0 startup;
  Out out;
  In in;
  instant_t t1;
  instant_t t2;
  instant_t t3;
  instant_t t4;
  Reaction *_reactions[CLOCK_SLAVE_NUM_REACTIONS];
  Trigger *_triggers[CLOCK_SLAVE_NUM_TRIGGERS];
} ClockSlave;

#endif
