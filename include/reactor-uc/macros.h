#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

// Sets an output port, copies data and triggers all downstream reactions.
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    typeof(val) __val = (val);                                                                                         \
    (port)->super.super.copy_value_and_trigger_downstreams(&(port)->super.super, (const void *)&__val, sizeof(__val)); \
  } while (0)

#define lf_get(trigger) (trigger)->buffer[((Trigger *)trigger)->trigger_value->read_idx]

// TODO: We need to handle the case when action already has been scheduled.
// then we need a runtime error and NOT overwrite the scheduled value.
#define lf_schedule(action, val, offset)                                                                               \
  do {                                                                                                                 \
    typeof(val) __val = (val);                                                                                         \
    (action)->super.super.schedule(&(action)->super.super, (offset), (const void *)&__val);                            \
  } while (0)
#endif
