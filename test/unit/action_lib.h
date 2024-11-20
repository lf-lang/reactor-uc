#ifndef ACTION_LIB_H
#define ACTION_LIB_H
#include "reactor-uc/reactor-uc.h"

#ifdef ACTION_LIB_VOID_TYPE
DEFINE_ACTION_STRUCT_VOID(ActionLib, act, LOGICAL_ACTION, 1, 1,0, 2);
DEFINE_ACTION_CTOR_VOID(ActionLib, act, LOGICAL_ACTION, 1, 1, 0, 2);
#else
DEFINE_ACTION_STRUCT(ActionLib, act, LOGICAL_ACTION, 1, 1, 0, 10, int);
DEFINE_ACTION_CTOR(ActionLib, act, LOGICAL_ACTION, 1, 1, 0, 10, int);
#endif

DEFINE_STARTUP_STRUCT(ActionLib, 1, 0);
DEFINE_STARTUP_CTOR(ActionLib);
DEFINE_REACTION_STRUCT(ActionLib, reaction, 1);
DEFINE_REACTION_CTOR(ActionLib, reaction, 0);

DEFINE_SHUTDOWN_STRUCT(ActionLib, 1, 0);
DEFINE_SHUTDOWN_CTOR(ActionLib);
DEFINE_REACTION_STRUCT(ActionLib, r_shutdown, 0)
DEFINE_REACTION_CTOR(ActionLib, r_shutdown, 1)


typedef struct {
  Reactor super;
  REACTION_INSTANCE(ActionLib, reaction);
  REACTION_INSTANCE(ActionLib, r_shutdown);
  SHUTDOWN_INSTANCE(ActionLib);
  ACTION_INSTANCE(ActionLib, act);
  STARTUP_INSTANCE(ActionLib);
  REACTOR_BOOKKEEPING_INSTANCES(2,3,0);
  int cnt;
} ActionLib;

REACTOR_CTOR_SIGNATURE(ActionLib) {
  REACTOR_CTOR_PREAMBLE();
  REACTOR_CTOR(ActionLib);

  INITIALIZE_REACTION(ActionLib, reaction);
  INITIALIZE_REACTION(ActionLib, r_shutdown);
  INITIALIZE_ACTION(ActionLib, act, MSEC(0));
  INITIALIZE_STARTUP(ActionLib);
  INITIALIZE_SHUTDOWN(ActionLib);
  ACTION_REGISTER_EFFECT(self->act, self->reaction);
  ACTION_REGISTER_SOURCE(self->act, self->reaction);
  STARTUP_REGISTER_EFFECT(self->reaction);
  SHUTDOWN_REGISTER_EFFECT(self->r_shutdown);

  self->cnt = 0;
}

void action_lib_start(interval_t duration) {
  ActionLib my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  ActionLib_ctor(&my_reactor, NULL, &env);
  env.scheduler.duration = duration;
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

#endif