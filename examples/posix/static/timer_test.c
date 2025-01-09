#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/static/scheduler.h"
#include "static_schedule.c"

LF_DEFINE_TIMER_STRUCT(TimerTest, t, 1, 0)
LF_DEFINE_TIMER_CTOR(TimerTest, t, 1, 0)
LF_DEFINE_REACTION_STRUCT(TimerTest, reaction, 0)
LF_DEFINE_REACTION_CTOR(TimerTest, reaction, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(TimerTest, reaction);
  LF_TIMER_INSTANCE(TimerTest, t);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1,1,0);
  int cnt;
} TimerTest;

LF_DEFINE_REACTION_BODY(TimerTest, reaction) {
  LF_SCOPE_SELF(TimerTest);
  LF_SCOPE_ENV();
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
  self->cnt++;
}

LF_REACTOR_CTOR_SIGNATURE(TimerTest) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(TimerTest);
  LF_INITIALIZE_REACTION(TimerTest, reaction);
  LF_INITIALIZE_TIMER(TimerTest, t, MSEC(0), MSEC(1));
  LF_TIMER_REGISTER_EFFECT(self->t, self->reaction);
}

TimerTest my_reactor;
Environment env;
void test_simple() {
  Environment_ctor(&env, (Reactor *)&my_reactor);
  env.scheduler->duration = MSEC(100);
  ((StaticScheduler*)env.scheduler)->static_schedule = NULL; // FIXME: register schedule here.
  TimerTest_ctor(&my_reactor, NULL, &env);
  env.assemble(&env);
  env.start(&env);
  Environment_free(&env);
}

int main() {
  test_simple();
  return 0;
}
