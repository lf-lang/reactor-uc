#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

// Sets an output port and triggers all downstream reactions.
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    (port)->value = (val);                                                                                             \
    (port)->super.super.trigger_downstreams(&(port)->super.super);                                                     \
  } while (0)

// TODO: We need to handle the case when action already has been scheduled.
// then we need a runtime error and NOT overwrite the scheduled value.
#define lf_schedule(action, val, offset)                                                                               \
  do {                                                                                                                 \
    assert(!(action)->super.super.super.is_scheduled);                                                                 \
    (action)->next_value = (val);                                                                                      \
    (action)->super.super.schedule(&(action)->super.super, (offset));                                                  \
  } while (0)
#endif
