#include "clock-master.h"

DEFINE_OUTPUT_PORT_CTOR(Out, 2);
DEFINE_ACTION_CTOR_FIXED(Action0, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1, MSEC(0));
DEFINE_OUTPUT_PORT_CTOR(Out, 1);
DEFINE_INPUT_PORT_CTOR(In, 1, ClockSyncMessage, 1);

// Startup reaction
DEFINE_REACTION_BODY(ClockMaster, 0) {
  ClockMaster *self = (ClockMaster *)_self->parent;
  Action0 *action = &self->action;
  Action0 *send_action = &self->send_action;
  Environment *env = self->super.env;

  // Schedule next round
  lf_schedule(action, CLOCK_SYNC, self->period);

  // Schedule the sending
  lf_schedule(send_action, CLOCK_SYNC, MSEC(0));
}
DEFINE_REACTION_CTOR(ClockMaster, 0);

// Reaction triggered by action. Should start a new round of clock sync.
DEFINE_REACTION_BODY(ClockMaster, 1) {
  ClockMaster *self = (ClockMaster *)_self->parent;
  Environment *env = self->super.env;
  Action0 *action = &self->action;
  Action0 *send_action = &self->send_action;
  assert(action->value == CLOCK_SYNC);

  // Schedule the sending
  lf_schedule(send_action, CLOCK_SYNC, MSEC(0));
}
DEFINE_REACTION_CTOR(ClockMaster, 1);

// Reaction triggered by input
DEFINE_REACTION_BODY(ClockMaster, 2) {
  ClockMaster *self = (ClockMaster *)_self->parent;
  Environment *env = self->super.env;
  In *in = &self->in;
  Action0 *send_action = &self->send_action;
  assert(in->value.type == CLOCK_DELAY_REQ);
  lf_schedule(send_action, CLOCK_DELAY_RESP, MSEC(0));
}
DEFINE_REACTION_CTOR(ClockMaster, 2);

// Reaction triggered by send action
DEFINE_REACTION_BODY(ClockMaster, 3) {
  ClockMaster *self = (ClockMaster *)_self->parent;
  Environment *env = self->super.env;
  Action0 *action = &self->send_action;
  Out *out = &self->out;

  ClockSyncMessage msg;
  msg.type = action->value;

  switch (action->value) {
  case CLOCK_SYNC:
    msg.time = env->get_physical_time(env);
    break;
  case CLOCK_DELAY_RESP:
    msg.time = env->get_logical_time(env);
    break;
  default:
    assert(0);
  }

  lf_set(out, msg);
}
DEFINE_REACTION_CTOR(ClockMaster, 3);