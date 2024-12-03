#include "board.h"
#include "reactor-uc/reactor-uc.h"
#include <inttypes.h>
#include "../../common/timer_source.h"


LF_DEFINE_REACTION_BODY(TimerSource, r) {
  LF_SCOPE_SELF(TimerSource);
  LF_SCOPE_ENV();
  printf("Hello World @ %lld\n", env->get_elapsed_logical_time(env));
  LED0_TOGGLE;
}

int main() {
  lf_start();
}
