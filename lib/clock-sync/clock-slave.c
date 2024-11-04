
#include "clock-slave.h"

DEFINE_OUTPUT_CTOR(Out, 2);
DEFINE_ACTION_CTOR_FIXED(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1, MSEC(0));
DEFINE_OUTPUT_CTOR(Out, 1);
DEFINE_INPUT_CTOR(In, 1, ClockSyncMessage, 1);
// TODO: Make sure that we dont overwrite timestamps before we have calculated the offset.
//  maybe the slave should request this whole thing, and not the master?

// Startup reaction
DEFINE_REACTION_BODY(ClockSlave, 0) {
  ClockSlave *self = (ClockSlave *)_self->parent;
  Action0 *send_action = &self->send_action;
  Environment *env = self->super.env;
}
DEFINE_REACTION_CTOR(ClockSlave, 0);

// Send reaction
DEFINE_REACTION_BODY(ClockSlave, 1) {
  ClockSlave *self = (ClockSlave *)_self->parent;
  Environment *env = self->super.env;

  Action0 *action = &self->send_action;
  Out *out = &self->out;
  assert(action->value == CLOCK_DELAY_REQ);

  ClockSyncMessage msg;
  msg.type = action->value;
  msg.time = 0;
  self->t3 = env->get_physical_time(env);
  lf_set(out, msg);
}
DEFINE_REACTION_CTOR(ClockSlave, 1);

// Reaction triggered by input
DEFINE_REACTION_BODY(ClockSlave, 2) {
  ClockSlave *self = (ClockSlave *)_self->parent;
  Environment *env = self->super.env;
  In *in = &self->in;
  Action0 *send_action = &self->send_action;

  if (in->value.type == CLOCK_SYNC) {
    self->t1 = in->value.time;
    self->t2 = env->get_logical_time(env);
    lf_schedule(send_action, CLOCK_DELAY_REQ, MSEC(0));
  } else if (in->value.type == CLOCK_DELAY_RESP) {
    self->t4 = in->value.time;
    // FIXME: Calculate offset and adjust clock.
  } else {
    assert(0);
  }
}
DEFINE_REACTION_CTOR(ClockSlave, 2);

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