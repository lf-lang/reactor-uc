#include "reactor-uc/reactor-uc.h"

DEFINE_ACTION_STRUCT(ActionTest, act, LOGICAL_ACTION, 1, 1, 2, int);
DEFINE_ACTION_CTOR(ActionTest, act, LOGICAL_ACTION, 1, 1, 2, int);
DEFINE_STARTUP_STRUCT(ActionTest, 1);
DEFINE_STARTUP_CTOR(ActionTest);
DEFINE_REACTION_STRUCT(ActionTest, reaction, 1);
DEFINE_REACTION_CTOR(ActionTest, reaction, 0);

typedef struct {
  Reactor super;
  REACTION_INSTANCE(ActionTest, reaction);
  ACTION_INSTANCE(ActionTest, act);
  STARTUP_INSTANCE(ActionTest);
  Reaction *_reactions[1];
  Trigger *_triggers[2];
  int cnt;
} ActionTest;


void ActionTest_ctor(ActionTest *self, Environment *env) {
  Reactor_ctor(&self->super, "ActionTest", env, NULL, NULL, 0, self->_reactions,
               sizeof(self->_reactions) / sizeof(self->_reactions[0]), self->_triggers,
               sizeof(self->_triggers) / sizeof(self->_triggers[0]));
  size_t _triggers_idx = 0;
  size_t _reactions_idx = 0;
  
  INITIALIZE_REACTION(ActionTest, reaction);
  INITIALIZE_ACTION(ActionTest, act, MSEC(0));
  INITIALIZE_STARTUP(ActionTest);
  ACTION_REGISTER_EFFECT(act, reaction);
  REACTION_REGISTER_EFFECT(reaction, act);
  ACTION_REGISTER_SOURCE(act, reaction);
  STARTUP_REGISTER_EFFECT(reaction);

  self->cnt = 0;
}

void action_lib_start(interval_t duration) {
  ActionTest my_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&my_reactor);
  ActionTest_ctor(&my_reactor, &env);
  env.scheduler.duration = duration;
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}
