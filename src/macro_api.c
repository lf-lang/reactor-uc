#include "reactor-uc/logging.h"
#include "reactor-uc/action.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/environment.h"
#include <assert.h>
#include <string.h>

/**
 * @brief this is a helper function for the lf_schedule macro with a value
 **/
lf_ret_t lf_schedule_with_value(Action* action, interval_t offset, const void* val) {
  lf_ret_t ret = action->schedule(action, offset, val);
  if (ret == LF_FATAL) {
    LF_ERR(TRIG, "Scheduling a value, for an void action!");
    Scheduler* sched = action->super.parent->env->scheduler;
    sched->do_shutdown(sched, sched->current_tag(sched));
    throw("Tried to schedule a value onto an action without a type!");
  }

  return ret;
}
