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

#define lf_schedule_with_val(action, offset, val)                                                                      \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    lf_ret_t ret = (action)->super.schedule(&(action)->super, (offset), (const void *)&__val);                         \
    if (ret == LF_FATAL) {                                                                                             \
      LF_ERR(TRIG, "Scheduling an value, that doesn't have value!");                                                   \
      Scheduler *sched = &(action)->super.super.parent->env->scheduler;                                                \
      sched->do_shutdown(sched, sched->current_tag);                                                                   \
      throw("Tried to schedule a value onto an action without a type!");                                               \
    }                                                                                                                  \
  } while (0)

#define lf_schedule_without_val(action, offset)                                                                        \
  do {                                                                                                                 \
    (action)->super.schedule(&(action)->super, (offset), NULL);                                                        \
  } while (0)

#define GET_ARG4(arg1, arg2, arg3, arg4, ...) arg4
#define LF_SCHEDULE_CHOOSER(...) GET_ARG4(__VA_ARGS__, lf_schedule_with_val, lf_schedule_without_val)

#define lf_schedule(...) LF_SCHEDULE_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

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

// Register a reaction as a source of a trigger. `trigger` must be a pointer to
// a derived Trigger type.
#define TRIGGER_REGISTER_SOURCE(trigger, source)                                                                       \
  do {                                                                                                                 \
    assert((trigger)->sources.num_registered < (trigger)->sources.size);                                               \
    (trigger)->sources.reactions[(trigger)->sources.num_registered++] = (source);                                      \
  } while (0)

// The following macros casts the inputs into the correct types before calling TRIGGER_REGISTER_EFFECTs
#define ACTION_REGISTER_EFFECT(TheAction, TheEffect)                                                                   \
  TRIGGER_REGISTER_EFFECT((Action *)&self->TheAction, (Reaction *)&self->TheEffect)

#define ACTION_REGISTER_SOURCE(TheAction, TheSource)                                                                   \
  TRIGGER_REGISTER_SOURCE((Action *)&self->TheAction, (Reaction *)&self->TheSource)

#define TIMER_REGISTER_EFFECT(TheTimer, TheEffect)                                                                     \
  TRIGGER_REGISTER_EFFECT((Timer *)(&(self->TheTimer)), (Reaction *)(&(self->TheEffect)))

#define STARTUP_REGISTER_EFFECT(effect)                                                                                \
  TRIGGER_REGISTER_EFFECT((BuiltinTrigger *)&(self->startup), (Reaction *)&self->effect)

#define INPUT_REGISTER_EFFECT(_Input, _Effect)                                                                         \
  TRIGGER_REGISTER_EFFECT((Input *)&self->_Input, (Reaction *)&self->_Effect)

#define SHUTDOWN_REGISTER_EFFECT(effect)                                                                               \
  TRIGGER_REGISTER_EFFECT((BuiltinTrigger *)&(self->shutdown), (Reaction *)&self->effect)

/**
 * @brief Convenience macro for registering a trigger as an effect of a reaction.
 *
 */
#define REACTION_REGISTER_EFFECT(_Reaction, _Effect)                                                                   \
  do {                                                                                                                 \
    Reaction *__reaction = (Reaction *)&self->_Reaction;                                                               \
    assert((__reaction)->effects_registered < (__reaction)->effects_size);                                             \
    (__reaction)->effects[(__reaction)->effects_registered++] = (Trigger *)&self->_Effect;                             \
  } while (0)

// Convenient translation from a user trigger to a pointer to the derived Trigger type.
#define OUTPUT_REGISTER_SOURCE(_Output, _Source)                                                                       \
  TRIGGER_REGISTER_SOURCE((Output *)&self->_Output, (Reaction *)&self->_Source);

// Convenience macro to register a downstream port on a connection.
// TODO: Replace entirely the need for `register_downstream`.
#define CONN_REGISTER_DOWNSTREAM(conn, down)                                                                           \
  do {                                                                                                                 \
    ((Connection *)&(conn))->register_downstream((Connection *)&(conn), (Port *)&(down));                              \
  } while (0)

#define BUNDLE_REGISTER_DOWNSTREAM(ReactorName, OtherName, InstanceName, Port)                                         \
  CONN_REGISTER_DOWNSTREAM(self->ReactorName##_##OtherName##_bundle.conn_##Port, self->InstanceName.Port);

#define BUNDLE_REGISTER_UPSTREAM(ReactorName, OtherName, InstanceName, Port)                                           \
  CONN_REGISTER_UPSTREAM(self->ReactorName##_##OtherName##_bundle.conn_##Port, self->InstanceName.Port);

// Convenience macro to register an upstream port on a connection
#define CONN_REGISTER_UPSTREAM(conn, up)                                                                               \
  do {                                                                                                                 \
    Port *_up = (Port *)&(up);                                                                                         \
    ((Connection *)&(conn))->upstream = _up;                                                                           \
    assert(_up->conns_out_registered < _up->conns_out_size);                                                           \
    _up->conns_out[_up->conns_out_registered++] = (Connection *)&(conn);                                               \
  } while (0)

// Convenience macro to register upstream and downstream on a connection
#define LOGICAL_CONNECT(SourceReactor, SourcePort, DestReactor, DestPort)                                              \
  CONN_REGISTER_UPSTREAM(self->conn_##SourcePort, self->SourceReactor.SourcePort);                                     \
  CONN_REGISTER_DOWNSTREAM(self->conn_##SourcePort, self->DestReactor.DestPort);

#define DELAYED_CONNECT(SourceReactor, SourcePort, DestReactor, DestPort)                                              \
  CONN_REGISTER_UPSTREAM(self->delayed_conn_##SourcePort, self->SourceReactor.SourcePort);                             \
  CONN_REGISTER_DOWNSTREAM(self->delayed_conn_##SourcePort, self->DestReactor.DestPort);

// Macros for creating the structs and ctors

#define REACTOR_CTOR_PREAMBLE()                                                                                        \
  size_t _reactions_idx = 0;                                                                                           \
  (void)_reactions_idx;                                                                                                \
  size_t _triggers_idx = 0;                                                                                            \
  (void)_triggers_idx;                                                                                                 \
  size_t _child_idx = 0;                                                                                               \
  (void)_child_idx;

#define FEDERATE_CTOR_PREAMBLE()                                                                                       \
  REACTOR_CTOR_PREAMBLE();                                                                                             \
  size_t _bundle_idx = 0;                                                                                              \
  (void)_bundle_idx;

#define REACTOR_CTOR(ReactorName)                                                                                      \
  Reactor_ctor(&self->super, __STRING(ReactorName), env, parent, self->_children,                                      \
               sizeof(self->_children) / sizeof(self->_children[0]), self->_reactions,                                 \
               sizeof(self->_reactions) / sizeof(self->_reactions[0]), self->_triggers,                                \
               sizeof(self->_triggers) / sizeof(self->_triggers[0]));

#define REACTOR_BOOKKEEPING_INSTANCES(NumReactions, NumTriggers, NumChildren)                                          \
  Reaction *_reactions[NumReactions];                                                                                  \
  Trigger *_triggers[NumTriggers];                                                                                     \
  Reactor *_children[NumChildren];

#define FEDERATE_CONNECTION_BUNDLE_INSTANCE(ReactorName, OtherName)                                                    \
  ReactorName##_##OtherName##_Bundle ReactorName##_##OtherName_bundle

#define FEDERATE_BOOKKEEPING_INSTANCES(NumReactions, NumTriggers, NumChildren, NumBundles)                             \
  REACTOR_BOOKKEEPING_INSTANCES(NumReactions, NumTriggers, NumChildren);                                               \
  FederatedConnectionBundle *_bundles[NumBundles];

#define DEFINE_OUTPUT_STRUCT(ReactorName, PortName, SourceSize)                                                        \
  typedef struct {                                                                                                     \
    Output super;                                                                                                      \
    Reaction *sources[(SourceSize)];                                                                                   \
  } ReactorName##_##PortName;

#define DEFINE_OUTPUT_CTOR(ReactorName, PortName, SourceSize)                                                          \
  void ReactorName##_##PortName##_ctor(ReactorName##_##PortName *self, Reactor *parent, Connection **conn_out,         \
                                       size_t conn_num) {                                                              \
    Output_ctor(&self->super, parent, self->sources, SourceSize, conn_out, conn_num);                                  \
  }

#define PORT_INSTANCE(ReactorName, PortName) ReactorName##_##PortName PortName;

#define INITIALIZE_OUTPUT(ReactorName, PortName, Conns, ConnSize)                                                      \
  ReactorName##_##PortName##_ctor(&self->PortName, &self->super, Conns, ConnSize)

#define INITIALIZE_INPUT(ReactorName, PortName)                                                                        \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->PortName;                                                       \
  ReactorName##_##PortName##_ctor(&self->PortName, &self->super)

#define DEFINE_INPUT_STRUCT(ReactorName, PortName, EffectSize, BufferType, NumConnsOut)                                \
  typedef struct {                                                                                                     \
    Input super;                                                                                                       \
    Reaction *effects[(EffectSize)];                                                                                   \
    BufferType value;                                                                                                  \
    Connection *conns_out[(NumConnsOut)];                                                                              \
  } ReactorName##_##PortName;

#define DEFINE_INPUT_CTOR(ReactorName, PortName, EffectSize, BufferType, NumConnsOut)                                  \
  void ReactorName##_##PortName##_ctor(ReactorName##_##PortName *self, Reactor *parent) {                              \
    Input_ctor(&self->super, parent, self->effects, (EffectSize), (Connection **)&self->conns_out, NumConnsOut,        \
               &self->value, sizeof(BufferType));                                                                      \
  }

#define DEFINE_TIMER_STRUCT(ReactorName, TimerName, EffectSize)                                                        \
  typedef struct {                                                                                                     \
    Timer super;                                                                                                       \
    Reaction *effects[(EffectSize)];                                                                                   \
  } ReactorName##_##TimerName;

#define DEFINE_TIMER_CTOR(ReactorName, TimerName, EffectSize)                                                          \
  void ReactorName##_##TimerName##_ctor(ReactorName##_##TimerName *self, Reactor *parent, interval_t offset,           \
                                        interval_t period) {                                                           \
    Timer_ctor(&self->super, parent, offset, period, self->effects, EffectSize);                                       \
  }

#define TIMER_INSTANCE(ReactorName, TimerName) ReactorName##_##TimerName TimerName;

#define INITIALIZE_TIMER(ReactorName, TimerName, Offset, Period)                                                       \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->TimerName;                                                      \
  ReactorName##_##TimerName##_ctor(&self->TimerName, &self->super, Offset, Period)

#define DEFINE_REACTION_STRUCT(ReactorName, ReactionName, EffectSize)                                                  \
  typedef struct {                                                                                                     \
    Reaction super;                                                                                                    \
    Trigger *effects[(EffectSize)];                                                                                    \
  } ReactorName##_Reaction_##ReactionName;

#define REACTION_INSTANCE(ReactorName, ReactionName) ReactorName##_Reaction_##ReactionName ReactionName;

#define INITIALIZE_REACTION(ReactorName, ReactionName)                                                                 \
  self->_reactions[_reactions_idx++] = (Reaction *)&self->ReactionName;                                                \
  ReactorName##_Reaction_##ReactionName##_ctor(&self->ReactionName, &self->super)

#define DEFINE_REACTION_BODY(ReactorName, ReactionName)                                                                \
  void ReactorName##_Reaction_##ReactionName##_body(Reaction *_self)

#define DEFINE_REACTION_CTOR(ReactorName, ReactionName, Priority)                                                      \
  DEFINE_REACTION_BODY(ReactorName, ReactionName);                                                                     \
  void ReactorName##_Reaction_##ReactionName##_ctor(ReactorName##_Reaction_##ReactionName *self, Reactor *parent) {    \
    Reaction_ctor(&self->super, parent, ReactorName##_Reaction_##ReactionName##_body, self->effects,                   \
                  sizeof(self->effects) / sizeof(self->effects[0]), Priority);                                         \
  }

#define DEFINE_STARTUP_STRUCT(ReactorName, EffectSize)                                                                 \
  typedef struct {                                                                                                     \
    BuiltinTrigger super;                                                                                              \
    Reaction *effects[(EffectSize)];                                                                                   \
  } ReactorName##_Startup;

#define DEFINE_STARTUP_CTOR(ReactorName)                                                                               \
  void ReactorName##_Startup_ctor(ReactorName##_Startup *self, Reactor *parent) {                                      \
    BuiltinTrigger_ctor(&self->super, TRIG_STARTUP, parent, self->effects,                                             \
                        sizeof(self->effects) / sizeof(self->effects[0]));                                             \
  }

#define STARTUP_INSTANCE(ReactorName) ReactorName##_Startup startup;
#define INITIALIZE_STARTUP(ReactorName)                                                                                \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->startup;                                                        \
  ReactorName##_Startup_ctor(&self->startup, &self->super)

#define DEFINE_SHUTDOWN_STRUCT(ReactorName, EffectSize)                                                                \
  typedef struct {                                                                                                     \
    BuiltinTrigger super;                                                                                              \
    Reaction *effects[(EffectSize)];                                                                                   \
  } ReactorName##_Shutdown;

#define DEFINE_SHUTDOWN_CTOR(ReactorName)                                                                              \
  void ReactorName##_Shutdown_ctor(ReactorName##_Shutdown *self, Reactor *parent) {                                    \
    BuiltinTrigger_ctor(&self->super, TRIG_SHUTDOWN, parent, self->effects,                                            \
                        sizeof(self->effects) / sizeof(self->effects[0]));                                             \
  }

#define SHUTDOWN_INSTANCE(ReactorName) ReactorName##_Shutdown shutdown;

#define INITIALIZE_SHUTDOWN(ReactorName)                                                                               \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->shutdown;                                                       \
  ReactorName##_Shutdown_ctor(&self->shutdown, &self->super)

#define DEFINE_ACTION_STRUCT(ReactorName, ActionName, ActionType, EffectSize, SourceSize, MaxPendingEvents,            \
                             BufferType)                                                                               \
  typedef struct {                                                                                                     \
    Action super;                                                                                                      \
    BufferType value;                                                                                                  \
    BufferType payload_buf[(MaxPendingEvents)];                                                                        \
    bool payload_used_buf[(MaxPendingEvents)];                                                                         \
    Reaction *sources[(SourceSize)];                                                                                   \
    Reaction *effects[(EffectSize)];                                                                                   \
  } ReactorName##_##ActionName;

#define DEFINE_ACTION_STRUCT_VOID(ReactorName, ActionName, ActionType, EffectSize, SourceSize, MaxPendingEvents)       \
  typedef struct {                                                                                                     \
    Action super;                                                                                                      \
    Reaction *sources[(SourceSize)];                                                                                   \
    Reaction *effects[(EffectSize)];                                                                                   \
  } ReactorName##_##ActionName;

#define DEFINE_ACTION_CTOR(ReactorName, ActionName, ActionType, EffectSize, SourceSize, MaxPendingEvents, BufferType)  \
  void ReactorName##_##ActionName##_ctor(ReactorName##_##ActionName *self, Reactor *parent, interval_t min_delay) {    \
    Action_ctor(&self->super, ActionType, min_delay, parent, self->sources, (SourceSize), self->effects, (EffectSize), \
                &self->value, sizeof(BufferType), (void *)&self->payload_buf, self->payload_used_buf,                  \
                (MaxPendingEvents));                                                                                   \
  }

#define DEFINE_ACTION_CTOR_VOID(ReactorName, ActionName, ActionType, EffectSize, SourceSize, MaxPendingEvents)         \
  void ReactorName##_##ActionName##_ctor(ReactorName##_##ActionName *self, Reactor *parent, interval_t min_delay) {    \
    Action_ctor(&self->super, ActionType, min_delay, parent, self->sources, (SourceSize), self->effects, (EffectSize), \
                NULL, 0, NULL, NULL, (MaxPendingEvents));                                                              \
  }

#define ACTION_INSTANCE(ReactorName, ActionName) ReactorName##_##ActionName ActionName;

#define INITIALIZE_ACTION(ReactorName, ActionName, MinDelay)                                                           \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->ActionName;                                                     \
  ReactorName##_##ActionName##_ctor(&self->ActionName, &self->super, MinDelay)

#define SCOPE_ACTION(ReactorName, ActionName) ReactorName##_##ActionName *ActionName = &self->ActionName

#define SCOPE_PORT(ReactorName, PortName) ReactorName##_##PortName *PortName = &self->PortName
#define SCOPE_SELF(ReactorName) ReactorName *self = (ReactorName *)_self->parent
#define SCOPE_ENV() Environment *env = self->super.env

#define DEFINE_LOGICAL_CONNECTION_STRUCT(ParentName, ReactorName, OutputPort, DownstreamSize)                          \
  typedef struct {                                                                                                     \
    LogicalConnection super;                                                                                           \
    Input *downstreams[(DownstreamSize)];                                                                              \
  } ParentName##_##ReactorName##_conn_##OutputPort;

#define DEFINE_LOGICAL_CONNECTION_CTOR(ParentName, ReactorName, OutputPort, DownstreamSize)                            \
  void ParentName##_##ReactorName##_conn_##OutputPort##_ctor(ParentName##_##ReactorName##_conn_##OutputPort *self,     \
                                                             Reactor *parent) {                                        \
    LogicalConnection_ctor(&self->super, parent, (Port **)self->downstreams,                                           \
                           sizeof(self->downstreams) / sizeof(self->downstreams[0]));                                  \
  }

#define LOGICAL_CONNECTION_INSTANCE(ParentName, ReactorName, OutputPort)                                               \
  ParentName##_##ReactorName##_conn_##OutputPort conn_##OutputPort;
#define CONTAINED_OUTPUT_CONNECTIONS(ReactorName, OutputPort, NumConnsOut)                                             \
  Connection *_conns_##ReactorName##_##OutputPort##_out[NumConnsOut];

#define INITIALIZE_LOGICAL_CONNECTION(ParentName, ReactorName, OutputPort)                                             \
  ParentName##_##ReactorName##_conn_##OutputPort##_ctor(&self->conn_##OutputPort, &self->super)

#define DEFINE_DELAYED_CONNECTION_STRUCT(ParentName, ReactorName, OutputPort, DownstreamSize, BufferType, BufferSize,  \
                                         Delay)                                                                        \
  typedef struct {                                                                                                     \
    DelayedConnection super;                                                                                           \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
    Input *downstreams[(BufferSize)];                                                                                  \
  } ParentName##_##ReactorName##_delayed_conn_##OutputPort;

#define DEFINE_DELAYED_CONNECTION_CTOR(ParentName, ReactorName, OutputPort, DownstreamSize, BufferType, BufferSize,    \
                                       Delay, IsPhysical)                                                              \
  void ParentName##_##ReactorName##_delayed_conn_##OutputPort##_ctor(                                                  \
      ParentName##_##ReactorName##_delayed_conn_##OutputPort *self, Reactor *parent) {                                 \
    DelayedConnection_ctor(&self->super, parent, (Port **)self->downstreams, DownstreamSize, Delay, IsPhysical,        \
                           sizeof(BufferType), (void *)self->payload_buf, self->payload_used_buf, BufferSize);         \
  }

#define DELAYED_CONNECTION_INSTANCE(ParentName, ReactorName, OutputPort)                                               \
  ParentName##_##ReactorName##_delayed_conn_##OutputPort delayed_conn_##OutputPort;

#define INITIALIZE_DELAYED_CONNECTION(ParentName, ReactorName, OutputPort)                                             \
  ParentName##_##ReactorName##_delayed_conn_##OutputPort##_ctor(&self->delayed_conn_##OutputPort, &self->super)

typedef struct FederatedOutputConnection FederatedOutputConnection;
#define DEFINE_FEDERATED_OUTPUT_CONNECTION(ReactorName, OutputName, BufferType, BufferSize)                            \
  typedef struct {                                                                                                     \
    FederatedOutputConnection super;                                                                                   \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
  } ReactorName##_##OutputName##_conn;                                                                                 \
                                                                                                                       \
  void ReactorName##_##OutputName##_conn_ctor(ReactorName##_##OutputName##_conn *self, Reactor *parent,                \
                                              FederatedConnectionBundle *bundle) {                                     \
    FederatedOutputConnection_ctor(&self->super, parent, bundle, 0, (void *)&self->payload_buf,                        \
                                   (bool *)&self->payload_used_buf, sizeof(BufferType), BufferSize);                   \
  }

#define FEDERATED_OUTPUT_CONNECTION_INSTANCE(ReactorName, OutputName)                                                  \
  ReactorName##_##OutputName##_conn conn_##OutputName

#define FEDERATED_CONNECTION_BUNDLE_INSTANCE(ReactorName, OtherName)                                                   \
  ReactorName##_##OtherName##_Bundle ReactorName##_##OtherName##_bundle

#define FEDERATED_CONNECTION_BUNDLE_NAME(ReactorName, OtherName) ReactorName##_##OtherName##_Bundle

#define FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(ReactorName, OtherName)                                             \
  void ReactorName##_##OtherName##_Bundle_ctor(ReactorName##_##OtherName##_Bundle *self, Reactor *parent)

// FIXME: Add  parent pointer
#define REACTOR_CTOR_SIGNATURE(ReactorName)                                                                            \
  void ReactorName##_ctor(ReactorName *self, Reactor *parent, Environment *env)

#define REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(ReactorName, ...)                                                       \
  void ReactorName##_ctor(ReactorName *self, Reactor *parent, Environment *env, __VA_ARGS__)

#define FEDERATED_CONNECTION_BUNDLE_CALL_CTOR()                                                                        \
  FederatedConnectionBundle_ctor(&self->super, parent, &self->channel.super, &self->inputs[0],                         \
                                 self->deserialize_hooks, sizeof(self->inputs) / sizeof(self->inputs[0]),              \
                                 &self->outputs[0], self->serialize_hooks,                                             \
                                 sizeof(self->outputs) / sizeof(self->outputs[0]));

#define INITIALIZE_FEDERATED_CONNECTION_BUNDLE(ReactorName, OtherName)                                                 \
  ReactorName##_##OtherName##_Bundle_ctor(&self->ReactorName##_##OtherName##_bundle, &self->super);                    \
  self->_bundles[_bundle_idx++] = &self->ReactorName##_##OtherName##_bundle.super;

#define INITIALIZE_FEDERATED_OUTPUT_CONNECTION(ReactorName, OutputName, SerializeFunc)                                 \
  ReactorName##_##OutputName##_conn_ctor(&self->conn_##OutputName, self->super.parent, &self->super);                  \
  self->outputs[_inputs_idx] = &self->conn_##OutputName.super;                                                         \
  self->serialize_hooks[_inputs_idx] = SerializeFunc;                                                                  \
  _outputs_idx++;

typedef struct FederatedInputConnection FederatedInputConnection;
#define DEFINE_FEDERATED_INPUT_CONNECTION(ReactorName, InputName, BufferType, BufferSize, Delay, IsPhysical)           \
  typedef struct {                                                                                                     \
    FederatedInputConnection super;                                                                                    \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
    Input *downstreams[1];                                                                                             \
  } ReactorName##_##InputName##_conn;                                                                                  \
                                                                                                                       \
  void ReactorName##_##InputName##_conn_ctor(ReactorName##_##InputName##_conn *self, Reactor *parent) {                \
    FederatedInputConnection_ctor(&self->super, parent, Delay, IsPhysical, (Port **)&self->downstreams, 1,             \
                                  (void *)&self->payload_buf, (bool *)&self->payload_used_buf, sizeof(BufferType),     \
                                  BufferSize);                                                                         \
  }

#define FEDERATED_INPUT_CONNECTION_INSTANCE(ReactorName, InputName) ReactorName##_##InputName##_conn conn_##InputName

#define INITIALIZE_FEDERATED_INPUT_CONNECTION(ReactorName, InputName, DeserializeFunc)                                 \
  ReactorName##_##InputName##_conn_ctor(&self->conn_##InputName, self->super.parent);                                  \
  self->inputs[_inputs_idx] = &self->conn_##InputName.super;                                                           \
  self->deserialize_hooks[_inputs_idx] = DeserializeFunc;                                                              \
  _inputs_idx++;

#define FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(NumInputs, NumOutputs)                                       \
  FederatedInputConnection *inputs[NumInputs];                                                                         \
  FederatedOutputConnection *outputs[NumOutputs];                                                                      \
  deserialize_hook deserialize_hooks[NumInputs];                                                                       \
  serialize_hook serialize_hooks[NumOutputs];

#define FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE()                                                                    \
  size_t _inputs_idx = 0;                                                                                              \
  (void)_inputs_idx;                                                                                                   \
  size_t _outputs_idx = 0;                                                                                             \
  (void)_outputs_idx;

#define CHILD_REACTOR_INSTANCE(ReactorName, instanceName) ReactorName instanceName;

#define INITIALIZE_CHILD_REACTOR(ReactorName, instanceName)                                                            \
  ReactorName##_ctor(&self->instanceName, &self->super, env);                                                          \
  self->_children[_child_idx++] = &self->instanceName.super;

#define INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(ReactorName, instanceName, ...)                                       \
  ReactorName##_ctor(&self->instanceName, &self->super, env, __VA_ARGS__);                                             \
  self->_children[_child_idx++] = &self->instanceName.super;

#define ENTRY_POINT(MainReactorName, Timeout, KeepAlive)                                                               \
  MainReactorName main_reactor;                                                                                        \
  Environment env;                                                                                                     \
  void lf_exit(void) { Environment_free(&env); }                                                                       \
  void lf_start() {                                                                                                    \
    Environment_ctor(&env, (Reactor *)&main_reactor);                                                                  \
    MainReactorName##_ctor(&main_reactor, NULL, &env);                                                                 \
    env.scheduler.duration = Timeout;                                                                                  \
    env.scheduler.keep_alive = KeepAlive;                                                                              \
    env.assemble(&env);                                                                                                \
    env.start(&env);                                                                                                   \
    lf_exit();                                                                                                         \
  }

#define ENTRY_POINT_FEDERATED(FederateName, Timeout, KeepAlive, HasInputs, NumBundles, IsLeader)                       \
  FederateName main_reactor;                                                                                           \
  Environment env;                                                                                                     \
  void lf_exit(void) { Environment_free(&env); }                                                                       \
  void lf_start() {                                                                                                    \
    Environment_ctor(&env, (Reactor *)&main_reactor);                                                                  \
    env.scheduler.duration = Timeout;                                                                                  \
    env.scheduler.keep_alive = KeepAlive;                                                                              \
    env.scheduler.leader = IsLeader;                                                                                   \
    env.has_async_events = HasInputs;                                                                                  \
    env.enter_critical_section(&env);                                                                                  \
    FederateName##_ctor(&main_reactor, NULL, &env);                                                                    \
    env.net_bundles_size = NumBundles;                                                                                 \
    env.net_bundles = (FederatedConnectionBundle **)&main_reactor._bundles;                                            \
    env.assemble(&env);                                                                                                \
    env.leave_critical_section(&env);                                                                                  \
    env.start(&env);                                                                                                   \
    lf_exit();                                                                                                         \
  }

// TODO: The following macro is defined to avoid compiler warnings. Ideally we would
// not have to specify any alignment on any structs. It is a TODO to understand exactly why
// the compiler complains and what we can do about it.
#define MEM_ALIGNMENT 32

#endif
