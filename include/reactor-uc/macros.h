#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

// Sets an output port, copies data and triggers all downstream reactions.
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    typeof(val) __val = (val);                                                                                         \
    (port)->super.super.copy_value_and_schedule_downstreams(&(port)->super.super, (const void *)&__val,                \
                                                            sizeof(__val));                                            \
  } while (0)

// FIXME: Needs to handle both actions (via the buffer) and input ports, which
//  are just direct.. how?
#define lf_get(trigger) (trigger)->buffer[((Trigger *)trigger)->trigger_value->read_idx]

// TODO: We need to handle the case when action already has been scheduled.
// then we need a runtime error and NOT overwrite the scheduled value.
#define lf_schedule(action, val, offset)                                                                               \
  do {                                                                                                                 \
    typeof(val) __val = (val);                                                                                         \
    (action)->super.super.schedule(&(action)->super.super, (offset), (const void *)&__val);                            \
  } while (0)
#endif

#define TRIGGER_REGISTER_EFFECT(trigger, effect)                                                                       \
  do {                                                                                                                 \
    assert((trigger)->effects.num_registered < (trigger)->effects.size);                                               \
    (trigger)->effects.reactions[(trigger)->effects.registered++] = (effect);                                          \
  } while (0)

#define TRIGGER_REGISTER_SOURCE(trigger, source)                                                                       \
  do {                                                                                                                 \
    assert((trigger)->sources.num_registered < (trigger)->sources.size);                                               \
    (trigger)->sources[(trigger)->sources.num_registered++] = (source);                                                \
  } while (0)
