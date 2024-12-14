#ifndef REACTOR_UC_MACROS_API_H
#define REACTOR_UC_MACROS_API_H

// Sets an output port, copies data and triggers all downstream reactions.
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    Port *_port = (Port *)(port);                                                                                      \
    _port->set(_port, &__val);                                                                                         \
  } while (0)

// Sets an output port, copies data and triggers all downstream reactions.
#define lf_set_array(port, array)                                                                                      \
  do {                                                                                                                 \
    Port *_port = (Port *)(port);                                                                                      \
    _port->set(_port, array);                                                                                          \
  } while (0)

/**
 * @brief Retreive the value of a trigger and cast it to the expected type
 */
#define lf_get(trigger) (&(trigger)->value)

/**
 * @brief Retrieve the is_present field of the trigger
 */
#define lf_is_present(trigger) (((Trigger *)(trigger))->is_present)

#define lf_schedule_with_val(action, offset, val)                                                                      \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    lf_ret_t ret = (action)->super.schedule(&(action)->super, (offset), (const void *)&__val);                         \
    if (ret == LF_FATAL) {                                                                                             \
      LF_ERR(TRIG, "Scheduling an value, that doesn't have value!");                                                   \
      Scheduler *sched = (action)->super.super.parent->env->scheduler;                                                 \
      sched->do_shutdown(sched, sched->current_tag(sched));                                                            \
      throw("Tried to schedule a value onto an action without a type!");                                               \
    }                                                                                                                  \
  } while (0)

#define lf_schedule_without_val(action, offset)                                                                        \
  do {                                                                                                                 \
    (action)->super.schedule(&(action)->super, (offset), NULL);                                                        \
  } while (0)

#endif
