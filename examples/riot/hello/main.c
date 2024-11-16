#include "reactor-uc/reactor-uc.h"

#include "../../common/timer_source.h" 

DEFINE_REACTION_BODY(TimerSource, r) {
  SCOPE_SELF(TimerSource);
  SCOPE_ENV();
  printf("TimerSource World @ %lld\n", env->get_elapsed_logical_time(env));
}

int main() {
  lf_start();
}