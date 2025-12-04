/**
 * @file macros_api.h
 * @author Erling Jellum (erling.jellum@gmail.com)
 * @brief Macros for setting and getting ports and scheduling actions, available inside reaction bodies.
 * @copyright Copyright (c) 2025 University of California, Berkeley
 */

#ifndef REACTOR_UC_MACROS_API_H
#define REACTOR_UC_MACROS_API_H

/**
 * @brief Sets the value of an output port and triggers all downstream reactions.
 *
 * This macro copies the value to the output port and consequently triggers all downstream reactions.

 * @param port The output port.
 * @param val The value to set.
 */
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    __typeof__((port)->value) __val = (val);                                                                           \
    Port* _port = (Port*)(port);                                                                                       \
    _port->set(_port, &__val);                                                                                         \
  } while (0)

/**
 * @brief Sets the value of an output port with an array,
 *
 * The port must have a type that is an array.
 *
 * @param port The output port.
 * @param array The array to set.
 */
#define lf_set_array(port, array)                                                                                      \
  do {                                                                                                                 \
    Port* _port = (Port*)(port);                                                                                       \
    _port->set(_port, array);                                                                                          \
  } while (0)

/**
 * @brief Get the value of an input port.
 *
 * This macro retrieves a pointer to the value of the input port.
 *
 * @param port The input port.
 */
#define lf_get(trigger) (&(trigger)->value)

/**
 * @brief Return whether the trigger is present at the current logical tag.
 *
 * @param trigger The trigger.
 * @returns True if the trigger is present, false otherwise.
 */
#define lf_is_present(trigger) (((Trigger*)(trigger))->is_present)

lf_ret_t lf_schedule_with_value(Action* action, interval_t offset, const void* val);

/// @private
#define lf_schedule_with_val(action, offset, val)                                                                      \
  ({                                                                                                                   \
    __typeof__(val) __val = (val);                                                                                     \
    Action* __a = (Action*)(action);                                                                                   \
    __a->schedule(__a, offset, &__val);                                                                                \
  })

/// @private
#define lf_schedule_without_val(action, offset)                                                                        \
  ({                                                                                                                   \
    Action* __a = (Action*)(action);                                                                                   \
    __a->schedule(__a, (offset), NULL);                                                                                \
  })

/// @private
#define GET_ARG4(arg1, arg2, arg3, arg4, ...) arg4

/// @private
#define LF_SCHEDULE_CHOOSER(...) GET_ARG4(__VA_ARGS__, lf_schedule_with_val, lf_schedule_without_val)

/**
 * @brief Schedule an action to occur at a future logical tag.
 *
 * This macro schedules an event on an action to occur at a future logical tag.
 * The macro can optionally be called with a value to be copied into the event
 * that is scheduled.
 *
 * @param action The action to schedule.
 * @param offset The offset from the current logical tag to schedule the event.
 * @param val (optional) The value to schedule with the event.
 */
#define lf_schedule(...) LF_SCHEDULE_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

/**
 * @brief Schedule an action with an array to occur at a future logical tag.
 *
 * This macro schedules an event with a payload consisting of a C araay, on an action to
 * occur at a future logical tag.
 *
 * @param action The action to schedule.
 * @param offset The offset from the current logical tag to schedule the event.
 * @param array The array to schedule as a payload of the event.
 */
#define lf_schedule_array(action, offset, array)                                                                       \
  ({                                                                                                                   \
    Action* __a = (Action*)(action);                                                                                   \
    lf_ret_t __ret = __a->schedule(__a, (offset), (const void*)array);                                                 \
  })

#endif
