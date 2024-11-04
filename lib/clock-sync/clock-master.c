#include "clock-master.h"

DEFINE_ACTION_CTOR_FIXED(ClockMaster, a_periodic, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1, MSEC(0));
DEFINE_ACTION_CTOR_FIXED(ClockMaster, a_send, LOGICAL_ACTION, 1, 1, ClockSyncMessageType, 1, MSEC(0));
DEFINE_OUTPUT_CTOR(ClockMaster, p_out, 1);
DEFINE_INPUT_CTOR(ClockMaster, p_in, 1, ClockSyncMessage, 1);

DEFINE_REACTION_CTOR(ClockMaster, r_startup, 0);
DEFINE_REACTION_CTOR(ClockMaster, r_action, 1);
DEFINE_REACTION_CTOR(ClockMaster, r_input, 2);
DEFINE_REACTION_CTOR(ClockMaster, r_send, 3);

// Startup reaction
DEFINE_REACTION_BODY(ClockMaster, r_startup) {
  SCOPE_SELF(ClockMaster);
  SCOPE_ACTION(ClockMaster, a_periodic);
  SCOPE_ACTION(ClockMaster, a_send);
  SCOPE_ENV();

  // Schedule next round
  lf_schedule(a_periodic, CLOCK_SYNC, self->period);

  // Schedule the sending
  lf_schedule(a_send, CLOCK_SYNC, MSEC(0));
}

// Reaction triggered by action. Should start a new round of clock sync.
DEFINE_REACTION_BODY(ClockMaster, r_action) {
  SCOPE_SELF(ClockMaster);
  SCOPE_ACTION(ClockMaster, a_send);
  SCOPE_ACTION(ClockMaster, a_periodic);
  assert(a_periodic->value == CLOCK_SYNC);

  // Schedule the sending
  lf_schedule(a_send, CLOCK_SYNC, MSEC(0));
}

// Reaction triggered by input
DEFINE_REACTION_BODY(ClockMaster, r_input) {
  SCOPE_SELF(ClockMaster);
  SCOPE_ENV();
  SCOPE_ACTION(ClockMaster, a_send);
  SCOPE_PORT(ClockMaster, p_in);

  assert(p_in->value.type == CLOCK_DELAY_REQ);
  lf_schedule(a_send, CLOCK_DELAY_RESP, MSEC(0));
}

// Reaction triggered by send action
DEFINE_REACTION_BODY(ClockMaster, r_send) {
  SCOPE_SELF(ClockMaster);
  SCOPE_ENV();
  SCOPE_ACTION(ClockMaster, a_send);
  SCOPE_PORT(ClockMaster, p_out);

  ClockSyncMessage msg;
  msg.type = a_send->value;

  switch (a_send->value) {
  case CLOCK_SYNC:
    msg.time = env->get_physical_time(env);
    break;
  case CLOCK_DELAY_RESP:
    msg.time = env->get_logical_time(env);
    break;
  default:
    assert(0);
  }

  lf_set(p_out, msg);
}

void ClockMaster_ctor(ClockMaster *self, Reactor *parent, Environment *env, Connection **conn_out, interval_t period) {
  int _reactions_idx = 0;
  int _triggers_idx = 0;
  int _child_idx = 0;

  INIT_REACTION(ClockMaster, r_startup);
  INIT_REACTION(ClockMaster, r_action);
  INIT_REACTION(ClockMaster, r_input);
  INIT_REACTION(ClockMaster, r_send);

  INIT_STARTUP(ClockMaster);
  INIT_ACTION(ClockMaster, a_send);
  INIT_ACTION(ClockMaster, a_periodic);
  INIT_OUTPUT(ClockMaster, p_out, conn_out, 1);
  INIT_INPUT(ClockMaster, p_in);

  self->period = period;

  Reactor_ctor(&self->super, "ClockSync", env, parent, self->_children, _child_idx, self->_reactions, _reactions_idx,
               self->_triggers, _triggers_idx);

  REACTION_TRIGGER(r_action, a_periodic);
  REACTION_EFFECT(r_action, r_send);

  REACTION_TRIGGER(r_input, p_in);
  REACTION_EFFECT(r_input, r_send);

  REACTION_TRIGGER(r_startup, startup);
  REACTION_EFFECT(r_startup, a_periodic);
  REACTION_EFFECT(r_startup, a_send);

  REACTION_TRIGGER(r_send, a_send);
  REACTION_EFFECT(r_send, p_out);
}