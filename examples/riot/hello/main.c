#include "reactor-uc/reactor-uc.h"

#include "../../common/timer_source.h" 

LF_DEFINE_REACTION_BODY(TimerSource, r) {
  LF_SCOPE_SELF(TimerSource);
  LF_SCOPE_ENV();
  printf("TimerSource World @ %lld\n", env->get_elapsed_logical_time(env));
}

int main() {
  lf_start();
}