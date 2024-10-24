#ifndef REACTOR_UC_MACROS_H
#define REACTOR_UC_MACROS_H

// Sets an output port, copies data and triggers all downstream reactions.
#define lf_set(port, val)                                                                                              \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    Port *_port = (Port *)(port);                                                                                      \
    for (size_t i = 0; i < _port->conns_out_registered; i++) {                                                         \
      Connection *__conn = _port->conns_out[i];                                                                        \
      __conn->trigger_downstreams(__conn, (const void *)&__val, sizeof(__val));                                        \
    }                                                                                                                  \
  } while (0)

/**
 * @brief Retreive the value of a trigger and cast it to the expected type
 */
#define lf_get(trigger) (&(trigger)->value)

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
    (action)->super.schedule(&(action)->super, (offset), (const void *)&__val);                                        \
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
#define BUILTIN_REGISTER_EFFECT(builtin, effect)                                                                       \
  TRIGGER_REGISTER_EFFECT((BuiltinTrigger *)&(builtin), (Reaction *)&(effect))
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
// TODO: Replace entirely the need for `register_downstream`.
#define CONN_REGISTER_DOWNSTREAM(conn, down)                                                                           \
  do {                                                                                                                 \
    ((Connection *)&(conn))->register_downstream((Connection *)&(conn), (Port *)&(down));                              \
  } while (0)

// Convenience macro to register an upstream port on a connection
#define CONN_REGISTER_UPSTREAM(conn, up)                                                                               \
  do {                                                                                                                 \
    Port *_up = (Port *)&(up);                                                                                         \
    ((Connection *)&(conn))->upstream = _up;                                                                           \
    assert(_up->conns_out_registered < _up->conns_out_size);                                                           \
    _up->conns_out[_up->conns_out_registered++] = (Connection *)&(conn);                                               \
  } while (0)

// Convenience macro to register upstream and downstream on a connection
#define CONNECT(ConnectionVariable, SourcePort, DestinationPort)                                                       \
  CONN_REGISTER_UPSTREAM(ConnectionVariable, SourcePort);                                                              \
  CONN_REGISTER_DOWNSTREAM(ConnectionVariable, DestinationPort)

#define DEFINE_OUTPUT_PORT_STRUCT(PortName, SourceSize, NumConnsOut)                                                   \
  typedef struct {                                                                                                     \
    Output super;                                                                                                      \
    Reaction *sources[(SourceSize)];                                                                                   \
    Connection *conns_out[NumConnsOut];                                                                                \
  } PortName;

#define DEFINE_OUTPUT_PORT_CTOR(PortName, SourceSize)                                                                  \
  void PortName##_ctor(PortName *self, Reactor *parent, Connection **conn_out, size_t conn_num) {                      \
    Output_ctor(&self->super, parent, self->sources, SourceSize, conn_out, conn_num);                                  \
  }

#define DEFINE_INPUT_PORT_STRUCT(PortName, EffectSize, BufferType, NumConnsOut)                                        \
  typedef struct {                                                                                                     \
    Input super;                                                                                                       \
    Reaction *effects[(EffectSize)];                                                                                   \
    BufferType value;                                                                                                  \
    Connection *conns_out[(NumConnsOut)];                                                                              \
  } PortName;

#define DEFINE_INPUT_PORT_CTOR(PortName, EffectSize, BufferType, NumConnsOut)                                          \
  void PortName##_ctor(PortName *self, Reactor *parent) {                                                              \
    Input_ctor(&self->super, parent, self->effects, (EffectSize), (Connection **)&self->conns_out, NumConnsOut,        \
               &self->value, sizeof(BufferType));                                                                      \
  }

#define DEFINE_TIMER_STRUCT(TimerName, EffectSize)                                                                     \
  typedef struct {                                                                                                     \
    Timer super;                                                                                                       \
    Reaction *effects[(EffectSize)];                                                                                   \
  } TimerName;

#define DEFINE_TIMER_CTOR(TimerName, EffectSize)                                                                       \
  void TimerName##_ctor(TimerName *self, Reactor *parent, interval_t offset, interval_t period) {                      \
    Timer_ctor(&self->super, parent, offset, period, self->effects, EffectSize);                                       \
  }

#define DEFINE_TIMER_CTOR_FIXED(TimerName, EffectSize, Offset, Period)                                                 \
  void TimerName##_ctor(TimerName *self, Reactor *parent) {                                                            \
    Timer_ctor(&self->super, parent, Offset, Period, self->effects, EffectSize);                                       \
  }

#define DEFINE_REACTION_STRUCT(ReactorName, ReactionIndex, EffectSize)                                                 \
  typedef struct {                                                                                                     \
    Reaction super;                                                                                                    \
    Trigger *effects[(EffectSize)];                                                                                    \
  } ReactorName##_Reaction##ReactionIndex;

#define DEFINE_REACTION_BODY(ReactorName, ReactionIndex)                                                               \
  void ReactorName##_Reaction##ReactionIndex##_body(Reaction *_self)

#define DEFINE_REACTION_CTOR(ReactorName, ReactionIndex)                                                               \
  void ReactorName##_Reaction##ReactionIndex##_ctor(ReactorName##_Reaction##ReactionIndex *self, Reactor *parent) {    \
    Reaction_ctor(&self->super, parent, ReactorName##_Reaction##ReactionIndex##_body, self->effects,                   \
                  sizeof(self->effects) / sizeof(self->effects[0]), ReactionIndex);                                    \
  }

#define DEFINE_STARTUP_STRUCT(StartupName, EffectSize)                                                                 \
  typedef struct {                                                                                                     \
    BuiltinTrigger super;                                                                                              \
    Reaction *effects[(EffectSize)];                                                                                   \
  } StartupName;

#define DEFINE_STARTUP_CTOR(StartupName, EffectSize)                                                                   \
  void StartupName##_ctor(StartupName *self, Reactor *parent) {                                                        \
    BuiltinTrigger_ctor(&self->super, TRIG_STARTUP, parent, self->effects,                                             \
                        sizeof(self->effects) / sizeof(self->effects[0]));                                             \
  }

#define DEFINE_SHUTDOWN_STRUCT(ShutdownName, EffectSize)                                                               \
  typedef struct {                                                                                                     \
    BuiltinTrigger super;                                                                                              \
    Reaction *effects[(EffectSize)];                                                                                   \
  } ShutdownName;

#define DEFINE_SHUTDOWN_CTOR(ShutdownName, EffectSize)                                                                 \
  void ShutdownName##_ctor(ShutdownName *self, Reactor *parent) {                                                      \
    BuiltinTrigger_ctor(&self->super, TRIG_SHUTDOWN, parent, self->effects,                                            \
                        sizeof(self->effects) / sizeof(self->effects[0]));                                             \
  }

#define DEFINE_ACTION_STRUCT(ActionName, ActionType, EffectSize, SourceSize, BufferType, BufferSize)                   \
  typedef struct {                                                                                                     \
    Action super;                                                                                                      \
    BufferType value;                                                                                                  \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
    Reaction *sources[(SourceSize)];                                                                                   \
    Reaction *effects[(EffectSize)];                                                                                   \
  } ActionName;

#define DEFINE_ACTION_CTOR_FIXED(ActionName, ActionType, EffectSize, SourceSize, BufferType, BufferSize, MinDelay)     \
  void ActionName##_ctor(ActionName *self, Reactor *parent) {                                                          \
    Action_ctor(&self->super, ActionType, MinDelay, parent, self->sources, SourceSize, self->effects, EffectSize,      \
                &self->value, sizeof(BufferType), (void *)&self->payload_buf, self->payload_used_buf, BufferSize);     \
  }

#define DEFINE_ACTION_CTOR(ActionName, ActionType, EffectSize, SourceSize, BufferType, BufferSize)                     \
  void ActionName##_ctor(ActionName *self, Reactor *parent, interval_t min_delay) {                                    \
    Action_ctor(&self->super, ActionType, min_delay, parent, self->sources, SourceSize, self->effects, EffectSize,     \
                &self->value, sizeof(BufferType), (void *)&self->payload_buf, self->payload_used_buf, BufferSize);     \
  }

#define DEFINE_LOGICAL_CONNECTION_STRUCT(ConnectionName, DownstreamSize)                                               \
  typedef struct {                                                                                                     \
    LogicalConnection super;                                                                                           \
    Input *downstreams[(DownstreamSize)];                                                                              \
  } ConnectionName;

#define DEFINE_LOGICAL_CONNECTION_CTOR(ConnectionName, DownstreamSize)                                                 \
  void ConnectionName##_ctor(ConnectionName *self, Reactor *parent) {                                                  \
    LogicalConnection_ctor(&self->super, parent, (Port **)self->downstreams,                                           \
                           sizeof(self->downstreams) / sizeof(self->downstreams[0]));                                  \
  }

#define DEFINE_DELAYED_CONNECTION_STRUCT(ConnectionName, DownstreamSize, BufferType, BufferSize, Delay)                \
  typedef struct {                                                                                                     \
    DelayedConnection super;                                                                                           \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
    Input *downstreams[(BufferSize)];                                                                                  \
  } ConnectionName;

#define DEFINE_DELAYED_CONNECTION_CTOR(ConnectionName, DownstreamSize, BufferType, BufferSize, Delay, IsPhysical)      \
  void ConnectionName##_ctor(ConnectionName *self, Reactor *parent) {                                                  \
    DelayedConnection_ctor(&self->super, parent, (Port **)self->downstreams, DownstreamSize, Delay, IsPhysical,        \
                           sizeof(BufferType), (void *)self->payload_buf, self->payload_used_buf, BufferSize);         \
  }

typedef struct FederatedOutputConnection FederatedOutputConnection;
#define DEFINE_FEDERATED_OUTPUT_CONNECTION(ConnectionName, BufferType, BufferSize)                                     \
  typedef struct {                                                                                                     \
    FederatedOutputConnection super;                                                                                   \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
  } ConnectionName;                                                                                                    \
                                                                                                                       \
  void ConnectionName##_ctor(ConnectionName *self, Reactor *parent, FederatedConnectionBundle *bundle) {               \
    FederatedOutputConnection_ctor(&self->super, parent, bundle, 0, (void *)&self->payload_buf,                        \
                                   (bool *)&self->payload_used_buf, sizeof(BufferType), BufferSize);                   \
  }

typedef struct FederatedInputConnection FederatedInputConnection;
#define DEFINE_FEDERATED_INPUT_CONNECTION(ConnectionName, DownstreamSize, BufferType, BufferSize, Delay, IsPhysical)   \
  typedef struct {                                                                                                     \
    FederatedInputConnection super;                                                                                    \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
    Input *downstreams[DownstreamSize];                                                                                \
  } ConnectionName;                                                                                                    \
                                                                                                                       \
  void ConnectionName##_ctor(ConnectionName *self, Reactor *parent) {                                                  \
    FederatedInputConnection_ctor(&self->super, parent, Delay, IsPhysical, (Port **)&self->downstreams,                \
                                  DownstreamSize, (void *)&self->payload_buf, (bool *)&self->payload_used_buf,         \
                                  sizeof(BufferType), BufferSize);                                                     \
  }

#define ENTRY_POINT(MainReactorName, Timeout, KeepAlive)                                                               \
  MainReactorName main_reactor;                                                                                        \
  Environment env;                                                                                                     \
  void lf_start() {                                                                                                    \
    Environment_ctor(&env, (Reactor *)&main_reactor);                                                                  \
    MainReactorName##_ctor(&main_reactor, &env);                                                                       \
    env.scheduler.set_timeout(&env.scheduler, Timeout);                                                                \
    env.assemble(&env);                                                                                                \
    env.scheduler.keep_alive = KeepAlive;                                                                              \
    env.start(&env);                                                                                                   \
  }

#define ENTRY_POINT_FEDERATED(FederateName, Timeout, KeepAlive, HasInputs, NumBundles)                                 \
  FederateName FederateName##_main;                                                                                    \
  Environment FederateName##_env;                                                                                      \
  void lf_##FederateName##_start() {                                                                                   \
    Environment *env = &FederateName##_env;                                                                            \
    FederateName *main = &FederateName##_main;                                                                         \
    Environment_ctor(env, (Reactor *)main);                                                                            \
    env->scheduler.set_timeout(&env->scheduler, Timeout);                                                              \
    env->scheduler.keep_alive = KeepAlive;                                                                             \
    env->has_async_events = HasInputs;                                                                                 \
    env->enter_critical_section(env);                                                                                  \
    FederateName##_ctor(main, env);                                                                                    \
    env->net_bundles_size = NumBundles;                                                                                \
    env->net_bundles = (FederatedConnectionBundle **)&main->_bundles;                                                  \
    env->assemble(env);                                                                                                \
    env->leave_critical_section(env);                                                                                  \
    env->start(env);                                                                                                   \
  }

// TODO: The following macro is defined to avoid compiler warnings. Ideally we would
// not have to specify any alignment on any structs. It is a TODO to understand exactly why
// the compiler complains and what we can do about it.
#define MEM_ALIGNMENT 32

#endif
