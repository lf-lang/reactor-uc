#ifndef REACTOR_UC_CLOCK_MASTER_H
#define REACTOR_UC_CLOCK_MASTER_H

#include "reactor-uc/reactor-uc.h"
#include "clock-sync.h"

DEFINE_ACTION_STRUCT(ClockMaster, a_send, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1);
DEFINE_ACTION_STRUCT(ClockMaster, a_periodic, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1);
DEFINE_STARTUP_STRUCT(ClockMaster, 1);
DEFINE_OUTPUT_STRUCT(ClockMaster, p_out, 2, 1);
DEFINE_INPUT_STRUCT(ClockMaster, p_in, 1, ClockSyncMessage, 1);

DEFINE_REACTION_STRUCT(ClockMaster, r_startup, 1);
DEFINE_REACTION_STRUCT(ClockMaster, r_action, 2);
DEFINE_REACTION_STRUCT(ClockMaster, r_send, 1);
DEFINE_REACTION_STRUCT(ClockMaster, r_input, 1);

#define CLOCK_MASTER_NUM_REACTIONS 4
#define CLOCK_MASTER_NUM_TRIGGERS 4
#define CLOCK_MASTER_NUM_CHILDREN 0

typedef struct {
  Reactor super;

  INSTANTIATE_REACTION(ClockMaster, r_startup);
  INSTANTIATE_REACTION(ClockMaster, r_action);
  INSTANTIATE_REACTION(ClockMaster, r_send);
  INSTANTIATE_REACTION(ClockMaster, r_input);
  INSTANTIATE_ACTION(ClockMaster, a_send);
  INSTANTIATE_ACTION(ClockMaster, a_periodic);
  INSTANTIATE_STARTUP(ClockMaster);
  INSTANTIATE_PORT(ClockMaster, p_out);
  INSTANTIATE_PORT(ClockMaster, p_in);

  Reaction *_reactions[CLOCK_MASTER_NUM_REACTIONS];
  Trigger *_triggers[CLOCK_MASTER_NUM_TRIGGERS];
  Reactor *_children[CLOCK_MASTER_NUM_CHILDREN];
  interval_t period;
} ClockMaster;

void ClockMaster_ctor(ClockMaster *self, Environment *env, Connection **conn_out, interval_t period);

#endif
