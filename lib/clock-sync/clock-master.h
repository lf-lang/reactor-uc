#ifndef REACTOR_UC_CLOCK_MASTER_H
#define REACTOR_UC_CLOCK_MASTER_H

#include "reactor-uc/reactor-uc.h"
#include "clock-sync.h"

DEFINE_ACTION_STRUCT(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1);
DECLARE_ACTION_CTOR_FIXED(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1, MSEC(0));
DEFINE_STARTUP_STRUCT(Startup0, 1);
DECLARE_STARTUP_CTOR(Startup0, 1);
DEFINE_OUTPUT_PORT_STRUCT(Out, 2, 1);
DECLARE_OUTPUT_PORT_CTOR(Out, 2);
DEFINE_INPUT_PORT_STRUCT(In, 1, ClockSyncMessage, 1);
DECLARE_INPUT_PORT_CTOR(In, 1, ClockSyncMessage, 1);

DEFINE_REACTION_STRUCT(ClockMaster, 0, 1);
DEFINE_REACTION_STRUCT(ClockMaster, 1, 2);
DEFINE_REACTION_STRUCT(ClockMaster, 2, 1);
DEFINE_REACTION_STRUCT(ClockMaster, 3, 1);

DECLARE_REACTION_CTOR(ClockMaster, 0);
DECLARE_REACTION_CTOR(ClockMaster, 1);
DECLARE_REACTION_CTOR(ClockMaster, 2);
DECLARE_REACTION_CTOR(ClockMaster, 3);

#define CLOCK_MASTER_NUM_REACTIONS 4
#define CLOCK_MASTER_NUM_TRIGGERS 5

typedef struct {
  Reactor super;
  ClockMaster_Reaction0 reaction0;
  ClockMaster_Reaction1 reaction1;
  ClockMaster_Reaction2 reaction2;
  ClockMaster_Reaction3 reaction3;
  Action0 action;
  Action0 send_action;
  Startup0 startup;
  Out out;
  In in;
  Reaction *_reactions[CLOCK_MASTER_NUM_REACTIONS];
  Trigger *_triggers[CLOCK_MASTER_NUM_TRIGGERS];
  interval_t period;
} ClockMaster;

void ClockMaster_ctor(ClockMaster *self, Environment *env, Connection **conn_out, interval_t period) {
  self->_reactions[0] = (Reaction *)&self->reaction0;
  self->_reactions[1] = (Reaction *)&self->reaction1;
  self->_reactions[2] = (Reaction *)&self->reaction2;
  self->_reactions[3] = (Reaction *)&self->reaction3;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->action;
  self->_triggers[2] = (Trigger *)&self->send_action;
  self->_triggers[3] = (Trigger *)&self->out;
  self->_triggers[4] = (Trigger *)&self->in;

  self->period = period;
  Reactor_ctor(&self->super, "ClockSync", env, NULL, NULL, 0, self->_reactions, CLOCK_MASTER_NUM_REACTIONS,
               self->_triggers, CLOCK_MASTER_NUM_TRIGGERS);

  ClockMaster_Reaction0_ctor(&self->reaction0, &self->super);
  ClockMaster_Reaction1_ctor(&self->reaction1, &self->super);
  ClockMaster_Reaction2_ctor(&self->reaction2, &self->super);
  ClockMaster_Reaction3_ctor(&self->reaction3, &self->super);

  Action0_ctor(&self->action, &self->super);
  Action0_ctor(&self->send_action, &self->super);
  Startup0_ctor(&self->startup, &self->super);
  In_ctor(&self->in, &self->super);
  Out_ctor(&self->out, &self->super, conn_out, 1);

  ACTION_REGISTER_EFFECT(self->action, self->reaction1);
  ACTION_REGISTER_EFFECT(self->send_action, self->reaction3);
  BUILTIN_REGISTER_EFFECT(self->startup, self->reaction0);
  INPUT_REGISTER_EFFECT(self->in, self->reaction2);
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction1);
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction2);
  ACTION_REGISTER_SOURCE(self->action, self->reaction1);
  REACTION_REGISTER_EFFECT(self->reaction0, self->action);
  REACTION_REGISTER_EFFECT(self->reaction1, self->action);
  REACTION_REGISTER_EFFECT(self->reaction1, self->send_action);
  REACTION_REGISTER_EFFECT(self->reaction2, self->send_action);
  REACTION_REGISTER_EFFECT(self->reaction3, self->out);
}

#endif
