#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

#define max(left, right)                                                                                                      \
  ({                                                                                                                   \
    __typeof__(left) _left = (left);                                                                                            \
    __typeof__(right) _right = (right);                                                                                            \
    _left > _right ? _left : _right;                                                                                                 \
  })

#define lf_set(port, input)                                                                                            \
  port->value = input;                                                                                                 \
  port->super.trigger_reactions(&port->super);
#define lf_get(port) port->value
#define lf_schedule(logical_action, interval_time, value)                                                              \
  logical_action->values[logical_action->current_value_write++] = value;                                               \
  logical_action->current_

#endif // REACTOR_UC_MACROS_H
