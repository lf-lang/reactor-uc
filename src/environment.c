#if defined(FEDERATED)
#include "./environments/unfederated_environment.c"
#include "./environments/federated_environment.c"
#else
#include "./environments/unfederated_environment.c"
#endif

#include "reactor-uc/timer.h"

void Environment_schedule_startups(const Environment *self, const tag_t start_tag) {
  if (self->startup) {
    LF_DEBUG(FED, "Scheduling Startup Reactions at" PRINTF_TAG, start_tag);
    Event event = EVENT_INIT(start_tag, &self->startup->super, NULL);
    LF_INFO(FED, "Self: %p Scheduler: %p", self, self->scheduler);
    lf_ret_t ret = self->scheduler->schedule_at(self->scheduler, &event);
    validate(ret == LF_OK);
  }
}

void Environment_schedule_timers(Environment *self, const Reactor *reactor, const tag_t start_tag) {
  lf_ret_t ret;
  for (size_t i = 0; i < reactor->triggers_size; i++) {
    Trigger *trigger = reactor->triggers[i];
    if (trigger->type == TRIG_TIMER) {
      Timer *timer = (Timer *)trigger;
      tag_t tag = {.time = start_tag.time + timer->offset, .microstep = start_tag.microstep};
      Event event = EVENT_INIT(tag, trigger, NULL);
      ret = self->scheduler->schedule_at(self->scheduler, &event);
      validate(ret == LF_OK);
    }
  }
  for (size_t i = 0; i < reactor->children_size; i++) {
    Environment_schedule_timers(self, reactor->children[i], start_tag);
  }
}
