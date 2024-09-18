#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

// Sets an output port and triggers all downstream reactions.
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    (port)->value = (val);                                                                                             \
    (port)->super.super.trigger_downstreams(&(port)->super.super);                                                     \
  } while (0)

// TODO: Fix this up, we probably need functions etc. for calculating next tag
// also, do we want to reference `self` here?
#define lf_schedule(action, val, offset)                                                                               \
  do {                                                                                                                 \
    (action)->next_value = val;                                                                                        \
    tag_t tag = {.time = self->super.env->current_tag.time + (action)->super.min_offset + offset, .microstep = 0};     \
    (action)->super.super.schedule_at(&(action)->super.super, tag);                                                    \
  } while (0)
#endif
