#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "unity.h"

// Wrap DynamicScheduler.clean_up_timestep to record how each tag is finalised.
static void (*orig_clean_up)(Scheduler*);
static int clean_up_calls = 0;
static tag_t clean_up_tags[8];
static bool clean_up_shutting_down[8];

static void counting_clean_up(Scheduler* s) {
  if (clean_up_calls < 8) {
    clean_up_tags[clean_up_calls] = ((DynamicScheduler*)s)->current_tag;
    clean_up_tags[clean_up_calls].time -= s->start_time;
    clean_up_shutting_down[clean_up_calls] = ((DynamicScheduler*)s)->is_shutting_down;
  }
  clean_up_calls++;
  orig_clean_up(s);
}

LF_DEFINE_TIMER_STRUCT(NoShutdown, t, 1, 0)
LF_DEFINE_TIMER_CTOR(NoShutdown, t, 1, 0)
LF_DEFINE_REACTION_STRUCT(NoShutdown, r, 0)
LF_DEFINE_REACTION_CTOR(NoShutdown, r, 0, NULL, NULL)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(NoShutdown, r);
  LF_TIMER_INSTANCE(NoShutdown, t);
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
  bool installed;
} NoShutdown;

LF_DEFINE_REACTION_BODY(NoShutdown, r) {
  LF_SCOPE_SELF(NoShutdown);
  LF_SCOPE_ENV();
  if (!self->installed) {
    DynamicScheduler* ds = (DynamicScheduler*)env->scheduler;
    orig_clean_up = ds->clean_up_timestep;
    ds->clean_up_timestep = counting_clean_up;
    self->installed = true;
  }
}

LF_REACTOR_CTOR_SIGNATURE(NoShutdown) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(NoShutdown);
  LF_INITIALIZE_REACTION(NoShutdown, r, NEVER);
  LF_INITIALIZE_TIMER(NoShutdown, t, MSEC(0), MSEC(1));
  LF_TIMER_REGISTER_EFFECT(self->t, self->r);
}

// 4 executed tags at 0,1,2,3 ms
LF_ENTRY_POINT(NoShutdown, 32, 32, MSEC(3), false, true)

void test_shutdown_tag_is_finalised(void) {
  // One per executed tag.
  TEST_ASSERT_EQUAL_MESSAGE(4, clean_up_calls, "every tag is finalised exactly once, the stop tag included");

  TEST_ASSERT_EQUAL_INT64_MESSAGE(MSEC(3), clean_up_tags[3].time, "the last finalised tag is the stop tag");
  TEST_ASSERT_TRUE_MESSAGE(clean_up_shutting_down[3], "the stop tag is finalised as the shutdown tag");
  TEST_ASSERT_FALSE_MESSAGE(clean_up_shutting_down[2], "an ordinary tag is not");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(lf_start);
  RUN_TEST(test_shutdown_tag_is_finalised);
  return UNITY_END();
}
