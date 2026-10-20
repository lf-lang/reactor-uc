#ifndef REACTOR_UC_TEST_EXTENSION_HOOKS_H
#define REACTOR_UC_TEST_EXTENSION_HOOKS_H

typedef struct {
  // How many times each hook ran.
  int tag_starts;
  int shutdown_tag_starts; // Tag starts with the shutdown flag set.
  int tag_completes;
  int shutdowns;
  int reaction_starts;
  int reaction_ends;
  int wait_starts;
  int wait_ends;
  int violations;
  int outcomes[LF_SCHEDULE_REJECTED + 1]; 

  // Ask the scheduler to visit `requested_tag`, and enqueue `reaction_at_requested_tag` there.
  bool request_tag;
  tag_t requested_tag;
  Reaction* reaction_at_requested_tag;
} TestHooks;

static inline void TestHooks_tag_start(TestHooks* self, Environment* env, tag_t tag, bool shutdown) {
  if (self == NULL) {
    return;
  }
  self->tag_starts++;
  self->shutdown_tag_starts += shutdown;
  if (self->request_tag && self->reaction_at_requested_tag != NULL && lf_tag_compare(tag, self->requested_tag) == 0) {
    LfExtension_enqueue_reaction(env, self->reaction_at_requested_tag);
  }
}

static inline tag_t TestHooks_next_tag(TestHooks* self, tag_t current) {
  if (self != NULL && self->request_tag && lf_tag_compare(self->requested_tag, current) > 0) {
    return self->requested_tag;
  }
  return FOREVER_TAG;
}

// Unbound environments pass a NULL context and count nothing.
#define TEST_HOOKS_COUNT(Context, Field)                                                                               \
  do {                                                                                                                 \
    if ((Context) != NULL) {                                                                                           \
      ((TestHooks*)(Context))->Field++;                                                                                \
    }                                                                                                                  \
  } while (0)

#define LF_EXTENSION_ON_TAG_START(Context, Env, Tag, Shutdown) TestHooks_tag_start(Context, Env, Tag, Shutdown)
#define LF_EXTENSION_ON_TAG_COMPLETE(Context, Env, Tag, Shutdown) TEST_HOOKS_COUNT(Context, tag_completes)
#define LF_EXTENSION_ON_SHUTDOWN(Context, Env) TEST_HOOKS_COUNT(Context, shutdowns)
#define LF_EXTENSION_ON_NEXT_TAG(Context, Env, Current) TestHooks_next_tag(Context, Current)
#define LF_EXTENSION_ON_REACTION_START(Context, Env, Reaction, Tag) TEST_HOOKS_COUNT(Context, reaction_starts)
#define LF_EXTENSION_ON_REACTION_END(Context, Env, Reaction, Tag) TEST_HOOKS_COUNT(Context, reaction_ends)
#define LF_EXTENSION_ON_ACTION_SCHEDULE(Context, Env, Action, Offset, Tag, Outcome)                                    \
  TEST_HOOKS_COUNT(Context, outcomes[Outcome])
#define LF_EXTENSION_ON_VIOLATION(Context, Env, Reaction, Tag, Kind) TEST_HOOKS_COUNT(Context, violations)
#define LF_EXTENSION_ON_WAIT_START(Context, Env, Target) TEST_HOOKS_COUNT(Context, wait_starts)
#define LF_EXTENSION_ON_WAIT_END(Context, Env, Target, Result) TEST_HOOKS_COUNT(Context, wait_ends)

#endif
