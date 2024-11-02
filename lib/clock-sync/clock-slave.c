
#include "clock-slave.h"

DEFINE_OUTPUT_PORT_CTOR(Out, 2);
DEFINE_ACTION_CTOR_FIXED(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1, MSEC(0));
DEFINE_OUTPUT_PORT_CTOR(Out, 1);
DEFINE_INPUT_PORT_CTOR(In, 1, ClockSyncMessage, 1);
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