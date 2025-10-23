#include "reactor-uc/reactor-uc.h"
#include "../../common/timer_source.h" 

LF_DEFINE_REACTION_BODY(TimerSource, r) {
  LF_SCOPE_SELF(TimerSource);
  LF_SCOPE_ENV();
  printf("TimerSource World @ " PRINTF_TIME "\n", env->get_elapsed_logical_time(env));
}

LF_DEFINE_REACTION_BODY(TimerSource, s) {
  LF_SCOPE_SELF(TimerSource);
  LF_SCOPE_ENV();
  LF_SCOPE_STARTUP(TimerSource);
  printf("TimerSource Startup @ " PRINTF_TIME "\n", env->get_elapsed_logical_time(env));
}


int main() {
  lf_start();
}