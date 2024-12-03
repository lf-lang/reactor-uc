#include "board.h"
#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>
#include "../../common/timer_source.h"


DEFINE_REACTION_BODY(TimerSource, r) {
  SCOPE_SELF(TimerSource);
  SCOPE_ENV();
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
  LED0_TOGGLE;
}

int main() {
  lf_start();
  return 0;
}
