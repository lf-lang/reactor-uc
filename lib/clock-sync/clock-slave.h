#ifndef REACTOR_UC_CLOCK_SLAVE_H
#define REACTOR_UC_CLOCK_SLAVE_H

#include "reactor-uc/reactor-uc.h"
#include "clock-sync.h"

DEFINE_ACTION_STRUCT(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1);
DECLARE_ACTION_CTOR_FIXED(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1, MSEC(0));
DEFINE_STARTUP_STRUCT(Startup0, 1);
DECLARE_STARTUP_CTOR(Startup0, 1);
DEFINE_OUTPUT_PORT_STRUCT(Out, 1, 1);
DECLARE_OUTPUT_PORT_CTOR(Out, 1);
DEFINE_INPUT_PORT_STRUCT(In, 1, ClockSyncMessage, 1);
DECLARE_INPUT_PORT_CTOR(In, 1, ClockSyncMessage, 1);

DEFINE_REACTION_STRUCT(ClockSlave, 0, 0);
DEFINE_REACTION_STRUCT(ClockSlave, 1, 1);
DEFINE_REACTION_STRUCT(ClockSlave, 2, 1);

DECLARE_REACTION_CTOR(ClockSlave, 0);
DECLARE_REACTION_CTOR(ClockSlave, 1);
DECLARE_REACTION_CTOR(ClockSlave, 2);

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
  Reaction *_reactions[CLOCK_SLAVE_NUM_REACTIONS];
  Trigger *_triggers[CLOCK_SLAVE_NUM_TRIGGERS];
  instant_t t1;
  instant_t t2;
  instant_t t3;
  instant_t t4;
} ClockSlave;

void ClockSlave_ctor(ClockSlave *self, Environment *env, Connection **conn_out) {
  self->_reactions[0] = (Reaction *)&self->reaction0;
  self->_reactions[1] = (Reaction *)&self->reaction1;
  self->_reactions[2] = (Reaction *)&self->reaction2;
  self->_triggers[0] = (Trigger *)&self->startup;
  self->_triggers[1] = (Trigger *)&self->send_action;
  self->_triggers[2] = (Trigger *)&self->out;
  self->_triggers[3] = (Trigger *)&self->in;

  Reactor_ctor(&self->super, "ClockSlave", env, NULL, NULL, 0, self->_reactions, CLOCK_SLAVE_NUM_REACTIONS,
               self->_triggers, CLOCK_SLAVE_NUM_TRIGGERS);

  ClockSlave_Reaction0_ctor(&self->reaction0, &self->super);
  ClockSlave_Reaction1_ctor(&self->reaction1, &self->super);
  ClockSlave_Reaction2_ctor(&self->reaction2, &self->super);

  Action0_ctor(&self->send_action, &self->super);
  Startup0_ctor(&self->startup, &self->super);
  In_ctor(&self->in, &self->super);
  Out_ctor(&self->out, &self->super, conn_out, 1);

  BUILTIN_REGISTER_EFFECT(self->startup, self->reaction0);
  ACTION_REGISTER_EFFECT(self->send_action, self->reaction1);
  INPUT_REGISTER_EFFECT(self->in, self->reaction2);
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction1);
  REACTION_REGISTER_EFFECT(self->reaction2, self->send_action);
  REACTION_REGISTER_EFFECT(self->reaction1, self->out);
}

#endif
