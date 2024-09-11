#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

#define max(a, b)                                                                                                      \
  ({                                                                                                                   \
    __typeof__(a) _a = (a);                                                                                            \
    __typeof__(b) _b = (b);                                                                                            \
    _a > _b ? _a : _b;                                                                                                 \
  })

#define lf_set(port, input)                                                                                            \
  port->value = input;                                                                                                 \
  port->super.trigger_reactions(&port->super);
#define lf_get(port) port->value
#define lf_schedule(logical_action, interval_time, value)                                                              \
  logical_action->values[logical_action->current_value_write++] = value;                                               \
  logical_action->current_

#endif // REACTOR_UC_MACROS_H
