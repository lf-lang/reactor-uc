#ifndef ACTION_LIB_H
#define ACTION_LIB_H
#include "reactor-uc/reactor-uc.h"

#ifdef ACTION_LIB_VOID_TYPE
LF_DEFINE_ACTION_STRUCT_VOID(ActionLib, act, LOGICAL_ACTION, 1, 1,0, 2);
LF_DEFINE_ACTION_CTOR_VOID(ActionLib, act, LOGICAL_ACTION, 1, 1, 0, 2);
#else
LF_DEFINE_ACTION_STRUCT(ActionLib, act, LOGICAL_ACTION, 1, 1, 0, 10, int);
LF_DEFINE_ACTION_CTOR(ActionLib, act, LOGICAL_ACTION, 1, 1, 0, 10, int);
#endif

LF_DEFINE_STARTUP_STRUCT(ActionLib, 1, 0);
LF_DEFINE_STARTUP_CTOR(ActionLib);
LF_DEFINE_REACTION_STRUCT(ActionLib, reaction, 1);
LF_DEFINE_REACTION_CTOR(ActionLib, reaction, 0, NULL, NEVER, NULL);

LF_DEFINE_SHUTDOWN_STRUCT(ActionLib, 1, 0);
LF_DEFINE_SHUTDOWN_CTOR(ActionLib);
LF_DEFINE_REACTION_STRUCT(ActionLib, r_shutdown, 0)
LF_DEFINE_REACTION_CTOR(ActionLib, r_shutdown, 1, NULL, NEVER, NULL);


typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(ActionLib, reaction);
  LF_REACTION_INSTANCE(ActionLib, r_shutdown);
  LF_SHUTDOWN_INSTANCE(ActionLib);
  LF_ACTION_INSTANCE(ActionLib, act);
  LF_STARTUP_INSTANCE(ActionLib);
  LF_REACTOR_BOOKKEEPING_INSTANCES(2,3,0);
  int cnt;
} ActionLib;

LF_REACTOR_CTOR_SIGNATURE(ActionLib) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(ActionLib);

  LF_INITIALIZE_REACTION(ActionLib, reaction);
  LF_INITIALIZE_REACTION(ActionLib, r_shutdown);
  LF_INITIALIZE_ACTION(ActionLib, act, MSEC(0), MSEC(0));
  LF_INITIALIZE_STARTUP(ActionLib);
  LF_INITIALIZE_SHUTDOWN(ActionLib);
  LF_ACTION_REGISTER_EFFECT(self->act, self->reaction);
  LF_ACTION_REGISTER_SOURCE(self->act, self->reaction);
  LF_STARTUP_REGISTER_EFFECT(self->reaction);
  LF_SHUTDOWN_REGISTER_EFFECT(self->r_shutdown);

  self->cnt = 0;
}
ActionLib my_reactor;
Environment env;
Environment *_lf_environment = &env;

void action_lib_start(interval_t duration) {
  Environment_ctor(&env, (Reactor *)&my_reactor, duration, false, false, false, NULL, 0, 0, NULL);
  ActionLib_ctor(&my_reactor, NULL, &env);
  env.start(&env);
  Environment_free(&env);
}

#endif