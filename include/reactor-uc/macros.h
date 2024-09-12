#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

#include "errors.h"

#define max(left, right)                                                                                               \
  ({                                                                                                                   \
    __typeof__(left) _left = (left);                                                                                   \
    __typeof__(right) _right = (right);                                                                                \
    _left > _right ? _left : _right;                                                                                   \
  })

#define lf_set(port, input)                                                                                            \
  ({                                                                                                                   \
    port->value = input;                                                                                               \
    port->super.trigger_reactions(&port->super);                                                                       \
  })

#define lf_get(port) ({ port->value; })

#define lf_schedule_value(logical_action, interval_time, value)                                                        \
  ({                                                                                                                   \
    if ((logical_action->current_value_write + 1) % logical_action->value_size ==                                      \
        logical_action->current_value_read) {                                                                          \
      NOTSCHEDULED;                                                                                                    \
    }                                                                                                                  \
    logical_action->values[logical_action->current_value_write] = value;                                               \
    logical_action->current_value_write = (logical_action->current_value_write + 1) % logical_action->value_size;      \
    LogicalAction_schedule(&logical_action->super, interval_time);                                                     \
  })

#define lf_schedule(logical_action, interval_time) ({ LogicalAction_schedule(&logical_action->super, interval_time); })

#define lf_action_get(logical_action)                                                                                  \
  ({                                                                                                                   \
    __typeof__(logical_action->values[0]) temp = logical_action->values[logical_action->current_value_read];           \
    logical_action->current_value_read = (logical_action->current_value_read + 1) % logical_action->value_size;        \
    temp;                                                                                                              \
  })

#endif // REACTOR_UC_MACROS_H
