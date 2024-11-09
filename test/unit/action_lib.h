#ifndef ACTION_LIB_H
#define ACTION_LIB_H
#include "reactor-uc/reactor-uc.h"

#ifdef ACTION_LIB_VOID_TYPE
DEFINE_ACTION_STRUCT_VOID(ActionLib, act, LOGICAL_ACTION, 1, 1, 2);
DEFINE_ACTION_CTOR_VOID(ActionLib, act, LOGICAL_ACTION, 1, 1, 2);
#else
DEFINE_ACTION_STRUCT(ActionLib, act, LOGICAL_ACTION, 1, 1, 2, int);
DEFINE_ACTION_CTOR(ActionLib, act, LOGICAL_ACTION, 1, 1, 2, int);
#endif

DEFINE_STARTUP_STRUCT(ActionLib, 1);
DEFINE_STARTUP_CTOR(ActionLib);
DEFINE_REACTION_STRUCT(ActionLib, reaction, 1);
DEFINE_REACTION_CTOR(ActionLib, reaction, 0);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(ActionLib, reaction);
  ACTION_INSTANCE(ActionLib, act);
  STARTUP_INSTANCE(ActionLib);
  Reaction *_reactions[1];
  Trigger *_triggers[2];
  int cnt;
} ActionLib;


void ActionIntLib_ctor(ActionLib *self, Environment *env) {
  Reactor_ctor(&self->super, "ActionLib", env, NULL, NULL, 0, self->_reactions,
               sizeof(self->_reactions) / sizeof(self->_reactions[0]), self->_triggers,
               sizeof(self->_triggers) / sizeof(self->_triggers[0]));
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  
  INITIALIZE_REACTION(ActionLib, reaction);
  INITIALIZE_ACTION(ActionLib, act, MSEC(0));
  INITIALIZE_STARTUP(ActionLib);
  ACTION_REGISTER_EFFECT(act, reaction);
  REACTION_REGISTER_EFFECT(reaction, act);
  ACTION_REGISTER_SOURCE(act, reaction);
  STARTUP_REGISTER_EFFECT(reaction);

  self->cnt = 0;
}

void action_int_lib_start(interval_t duration) {
  ActionLib my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  ActionIntLib_ctor(&my_reactor, &env);
  env.scheduler.duration = duration;
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

#endif