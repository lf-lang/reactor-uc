#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

// Sets an output port, copies data and triggers all downstream reactions.
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    Connection *__conn = (port)->super.super.conn_out;                                                                 \
    if (__conn) {                                                                                                      \
      __conn->trigger_downstreams(__conn, (const void *)&__val, sizeof(__val));                                        \
    }                                                                                                                  \
  } while (0)

/**
 * @brief Retreive the value of a trigger and cast it to the expected type
 * FIXME: Handle errors
 *
 */
#define lf_get(trigger) (*(__typeof__((trigger)->buffer[0]) *)(((Trigger *)(trigger))->get((Trigger *)(trigger))))

/**
 * @brief Retrieve the is_present field of the trigger
 */
#define lf_is_present(trigger) (((Trigger *)(trigger))->is_present)

// TODO: We need to handle the case when action already has been scheduled.
// then we need a runtime error and NOT overwrite the scheduled value.
/**
 * @brief Schedule an event on an action
 *
 */
#define lf_schedule(action, val, offset)                                                                               \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    (action)->super.super.schedule(&(action)->super.super, (offset), (const void *)&__val);                            \
  } while (0)

/**
 * @brief Convenience macro for registering a reaction as an effect of a trigger.
 * The input must be a pointer to a derived Trigger type with an effects field.
 * E.g. Action, Timer, Startup, Shutdown, Input
 *
 */
#define TRIGGER_REGISTER_EFFECT(trigger, effect)                                                                       \
  do {                                                                                                                 \
    assert((trigger)->effects.num_registered < (trigger)->effects.size);                                               \
    (trigger)->effects.reactions[(trigger)->effects.num_registered++] = (effect);                                      \
  } while (0)

// The following macros casts the inputs into the correct types before calling TRIGGER_REGISTER_EFFECTs
#define ACTION_REGISTER_EFFECT(action, effect) TRIGGER_REGISTER_EFFECT((Action *)&(action), (Reaction *)&(effect))
#define TIMER_REGISTER_EFFECT(timer, effect) TRIGGER_REGISTER_EFFECT((Timer *)(&(timer)), (Reaction *)(&(effect)))
#define STARTUP_REGISTER_EFFECT(startup, effect) TRIGGER_REGISTER_EFFECT((Startup *)&(startup), (Reaction *)&(effect))
#define SHUTDOWN_REGISTER_EFFECT(shutdown, effect)                                                                     \
  TRIGGER_REGISTER_EFFECT((Shutdown *)&(shutdown), (Reaction *)&(effect))
#define INPUT_REGISTER_EFFECT(input, effect) TRIGGER_REGISTER_EFFECT((Input *)&(input), (Reaction *)&(effect))

/**
 * @brief Convenience macro for registering a trigger as an effect of a reaction.
 *
 */
#define REACTION_REGISTER_EFFECT(reaction, effect)                                                                     \
  do {                                                                                                                 \
    Reaction *__reaction = (Reaction *)&(reaction);                                                                    \
    assert((__reaction)->effects_registered < (__reaction)->effects_size);                                             \
    (__reaction)->effects[(__reaction)->effects_registered++] = (Trigger *)&(effect);                                  \
  } while (0)

// Register a reaction as a source of a trigger. `trigger` must be a pointer to
// a derived Trigger type.
#define TRIGGER_REGISTER_SOURCE(trigger, source)                                                                       \
  do {                                                                                                                 \
    assert((trigger)->sources.num_registered < (trigger)->sources.size);                                               \
    (trigger)->sources.reactions[(trigger)->sources.num_registered++] = (source);                                      \
  } while (0)

// Convenient translation from a user trigger to a pointer to the derived Trigger type.
#define ACTION_REGISTER_SOURCE(action, source) TRIGGER_REGISTER_SOURCE((Action *)&(action), (Reaction *)&(source))
#define OUTPUT_REGISTER_SOURCE(output, source) TRIGGER_REGISTER_SOURCE((Output *)&(output), (Reaction *)&(source))

// Convenience macro to register a downstream port on a connection.
#define CONN_REGISTER_DOWNSTREAM(conn, down)                                                                           \
  do {                                                                                                                 \
    ((Connection *)&(conn))->register_downstream((Connection *)&(conn), (Port *)&(down));                              \
  } while (0)

// Convenience macro to register an upstream port on a connection
#define CONN_REGISTER_UPSTREAM(conn, up)                                                                               \
  do {                                                                                                                 \
    ((Connection *)&(conn))->upstream = (Port *)&(up);                                                                 \
  } while (0)

// TODO: The following macro is defined to avoid compiler warnings. Ideally we would
// not have to specify any alignment on any structs. It is a TODO to understand exactly why
// the compiler complains and what we can do about it.
#define MEM_ALIGNMENT 32
#endif
