#ifndef REACTOR_UC_MACROS_INTERNAL_H
#define REACTOR_UC_MACROS_INTERNAL_H

#define LF_STRINGIFY(x) #x

/**
 * @brief Convenience macro for registering a reaction as an effect of a trigger.
 * The input must be a pointer to a derived Trigger type with an effects field.
 * E.g. Action, Timer, Startup, Shutdown, Input
 *
 */
#define LF_TRIGGER_REGISTER_EFFECT(trigger, effect)                                                                    \
  do {                                                                                                                 \
    assert((trigger)->effects.num_registered < (trigger)->effects.size);                                               \
    (trigger)->effects.reactions[(trigger)->effects.num_registered++] = (effect);                                      \
  } while (0)

// Register a reaction as a source of a trigger. `trigger` must be a pointer to
// a derived Trigger type.
#define LF_TRIGGER_REGISTER_SOURCE(trigger, source)                                                                    \
  do {                                                                                                                 \
    assert((trigger)->sources.num_registered < (trigger)->sources.size);                                               \
    (trigger)->sources.reactions[(trigger)->sources.num_registered++] = (source);                                      \
  } while (0)

// Register a reaction as a source of a trigger. `trigger` must be a pointer to
// a derived Trigger type.
#define LF_TRIGGER_REGISTER_OBSERVER(trigger, observer)                                                                \
  do {                                                                                                                 \
    assert((trigger)->observers.num_registered < (trigger)->observers.size);                                           \
    (trigger)->observers.reactions[(trigger)->observers.num_registered++] = (observer);                                \
  } while (0)

// The following macros casts the inputs into the correct types before calling TRIGGER_REGISTER_EFFECTs
#define LF_ACTION_REGISTER_EFFECT(TheAction, TheEffect)                                                                \
  LF_TRIGGER_REGISTER_EFFECT((Action *)&(TheAction), (Reaction *)&(TheEffect))

#define LF_ACTION_REGISTER_SOURCE(TheAction, TheSource)                                                                \
  LF_TRIGGER_REGISTER_SOURCE((Action *)&(TheAction), (Reaction *)&(TheSource));                                        \
  LF_REACTION_REGISTER_EFFECT(TheSource, TheAction);

#define LF_ACTION_REGISTER_OBSERVER(TheAction, TheObserver)                                                            \
  LF_TRIGGER_REGISTER_OBSERVER((Action *)&(TheAction), (Reaction *)&(TheObserver))

#define LF_TIMER_REGISTER_EFFECT(TheTimer, TheEffect)                                                                  \
  LF_TRIGGER_REGISTER_EFFECT((Timer *)(&(TheTimer)), (Reaction *)(&(TheEffect)))

#define LF_TIMER_REGISTER_OBSERVER(TheTimer, TheObserver)                                                              \
  LF_TRIGGER_REGISTER_OBSERVER((Timer *)(&(TheTimer)), (Reaction *)(&(TheObserver)))

#define LF_STARTUP_REGISTER_EFFECT(TheEffect)                                                                          \
  LF_TRIGGER_REGISTER_EFFECT((BuiltinTrigger *)&(self->startup), (Reaction *)&(TheEffect))

#define LF_STARTUP_REGISTER_OBSERVER(TheObserver)                                                                      \
  LF_TRIGGER_REGISTER_OBSERVER((BuiltinTrigger *)&(self->startup), (Reaction *)&(TheObserver))

#define LF_PORT_REGISTER_EFFECT(ThePort, TheEffect, PortWidth)                                                         \
  for (int i = 0; i < PortWidth; i++) {                                                                                \
    LF_TRIGGER_REGISTER_EFFECT((Port *)&(ThePort)[i], (Reaction *)&(TheEffect));                                       \
  }

#define LF_PORT_REGISTER_SOURCE(ThePort, TheSource, PortWidth)                                                         \
  for (int i = 0; i < PortWidth; i++) {                                                                                \
    LF_TRIGGER_REGISTER_SOURCE((Port *)&(ThePort)[i], (Reaction *)&(TheSource));                                       \
    LF_REACTION_REGISTER_EFFECT(TheSource, ThePort[i]);                                                                \
  }

#define LF_PORT_REGISTER_OBSERVER(ThePort, TheObserver, PortWidth)                                                     \
  for (int i = 0; i < PortWidth; i++) {                                                                                \
    LF_TRIGGER_REGISTER_OBSERVER((Port *)&(ThePort)[i], (Reaction *)&(TheObserver));                                   \
  }

#define LF_SHUTDOWN_REGISTER_EFFECT(TheEffect)                                                                         \
  LF_TRIGGER_REGISTER_EFFECT((BuiltinTrigger *)&(self->shutdown), (Reaction *)&(TheEffect))

#define LF_SHUTDOWN_REGISTER_OBSERVER(TheObserver)                                                                     \
  LF_TRIGGER_REGISTER_OBSERVER((BuiltinTrigger *)&(self->shutdown), (Reaction *)&(TheObserver))

/**
 * @brief Convenience macro for registering a trigger as an effect of a reaction.
 *
 */
#define LF_REACTION_REGISTER_EFFECT(TheReaction, TheEffect)                                                            \
  do {                                                                                                                 \
    Reaction *__reaction = (Reaction *)&(TheReaction);                                                                 \
    assert((__reaction)->effects_registered < (__reaction)->effects_size);                                             \
    (__reaction)->effects[(__reaction)->effects_registered++] = (Trigger *)&(TheEffect);                               \
  } while (0)

// Macros for creating the structs and ctors
#define LF_REACTOR_CTOR_PREAMBLE()                                                                                     \
  size_t _reactions_idx = 0;                                                                                           \
  (void)_reactions_idx;                                                                                                \
  size_t _triggers_idx = 0;                                                                                            \
  (void)_triggers_idx;                                                                                                 \
  size_t _child_idx = 0;                                                                                               \
  (void)_child_idx;

#define LF_FEDERATE_CTOR_PREAMBLE()                                                                                    \
  LF_REACTOR_CTOR_PREAMBLE();                                                                                          \
  size_t _bundle_idx = 0;                                                                                              \
  (void)_bundle_idx;

#define LF_REACTOR_CTOR(ReactorName)                                                                                   \
  Reactor_ctor(&self->super, LF_STRINGIFY(ReactorName), env, parent, self->_children,                                  \
               sizeof(self->_children) / sizeof(self->_children[0]), self->_reactions,                                 \
               sizeof(self->_reactions) / sizeof(self->_reactions[0]), self->_triggers,                                \
               sizeof(self->_triggers) / sizeof(self->_triggers[0]));

#define LF_REACTOR_BOOKKEEPING_INSTANCES(NumReactions, NumTriggers, NumChildren)                                       \
  Reaction *_reactions[NumReactions];                                                                                  \
  Trigger *_triggers[NumTriggers];                                                                                     \
  Reactor *_children[NumChildren];

#define LF_FEDERATE_CONNECTION_BUNDLE_INSTANCE(ReactorName, OtherName)                                                 \
  ReactorName##_##OtherName##_Bundle ReactorName##_##OtherName_bundle

#define LF_FEDERATE_BOOKKEEPING_INSTANCES(NumBundles)                                                                  \
  LF_REACTOR_BOOKKEEPING_INSTANCES(0, 0, 1);                                                                           \
  FederatedConnectionBundle *_bundles[NumBundles];

#define LF_DEFINE_OUTPUT_STRUCT(ReactorName, PortName, SourceSize, BufferType)                                         \
  typedef struct {                                                                                                     \
    Port super;                                                                                                        \
    Reaction *sources[(SourceSize)];                                                                                   \
    BufferType value;                                                                                                  \
  } ReactorName##_##PortName;

#define LF_DEFINE_OUTPUT_ARRAY_STRUCT(ReactorName, PortName, SourceSize, BufferType, ArrayLen)                         \
  typedef struct {                                                                                                     \
    Port super;                                                                                                        \
    Reaction *sources[(SourceSize)];                                                                                   \
    BufferType value[(ArrayLen)];                                                                                      \
  } ReactorName##_##PortName;

#define LF_DEFINE_OUTPUT_CTOR(ReactorName, PortName, SourceSize)                                                       \
  void ReactorName##_##PortName##_ctor(ReactorName##_##PortName *self, Reactor *parent,                                \
                                       OutputExternalCtorArgs external) {                                              \
    Port_ctor(&self->super, TRIG_OUTPUT, parent, &self->value, sizeof(self->value), external.parent_effects,           \
              external.parent_effects_size, self->sources, SourceSize, external.parent_observers,                      \
              external.parent_observers_size, external.conns_out, external.conns_out_size);                            \
  }

#define LF_PORT_INSTANCE(ReactorName, PortName, PortWidth) ReactorName##_##PortName PortName[PortWidth];
#define LF_PORT_PTR_INSTANCE(ReactorName, PortName) ReactorName##_##PortName *PortName;
#define LF_MULTIPORT_PTR_INSTANCE(ReactorName, PortName, PortWidth)                                                    \
  ReactorName##_##PortName *PortName[PortWidth];                                                                       \
  int PortName##_width;

#define LF_INITIALIZE_OUTPUT(ReactorName, PortName, PortWidth, External)                                               \
  for (int i = 0; i < (PortWidth); i++) {                                                                              \
    self->_triggers[_triggers_idx++] = (Trigger *)&self->PortName[i];                                                  \
    ReactorName##_##PortName##_ctor(&self->PortName[i], &self->super, External[i]);                                    \
  }

#define LF_INITIALIZE_INPUT(ReactorName, PortName, PortWidth, External)                                                \
  for (int i = 0; i < (PortWidth); i++) {                                                                              \
    self->_triggers[_triggers_idx++] = (Trigger *)&self->PortName[i];                                                  \
    ReactorName##_##PortName##_ctor(&self->PortName[i], &self->super, External[i]);                                    \
  }

#define LF_DEFINE_INPUT_STRUCT(ReactorName, PortName, EffectSize, ObserversSize, BufferType, NumConnsOut)              \
  typedef struct {                                                                                                     \
    Port super;                                                                                                        \
    Reaction *effects[(EffectSize)];                                                                                   \
    Reaction *observers[(ObserversSize)];                                                                              \
    BufferType value;                                                                                                  \
    Connection *conns_out[(NumConnsOut)];                                                                              \
  } ReactorName##_##PortName;

#define LF_DEFINE_INPUT_ARRAY_STRUCT(ReactorName, PortName, EffectSize, ObserversSize, BufferType, ArrayLen,           \
                                     NumConnsOut)                                                                      \
  typedef struct {                                                                                                     \
    Port super;                                                                                                        \
    Reaction *effects[(EffectSize)];                                                                                   \
    Reaction *observers[(ObserversSize)];                                                                              \
    BufferType value[(ArrayLen)];                                                                                      \
    Connection *conns_out[(NumConnsOut)];                                                                              \
  } ReactorName##_##PortName;

#define LF_DEFINE_INPUT_CTOR(ReactorName, PortName, EffectSize, ObserverSize, BufferType, NumConnsOut)                 \
  void ReactorName##_##PortName##_ctor(ReactorName##_##PortName *self, Reactor *parent,                                \
                                       InputExternalCtorArgs external) {                                               \
    Port_ctor(&self->super, TRIG_INPUT, parent, &self->value, sizeof(self->value), self->effects, (EffectSize),        \
              external.parent_sources, external.parent_sources_size, self->observers, ObserverSize,                    \
              (Connection **)&self->conns_out, NumConnsOut);                                                           \
  }

#define LF_DEFINE_TIMER_STRUCT(ReactorName, TimerName, EffectSize, ObserversSize)                                      \
  typedef struct {                                                                                                     \
    Timer super;                                                                                                       \
    Reaction *effects[(EffectSize)];                                                                                   \
    Reaction *observers[(ObserversSize)];                                                                              \
  } ReactorName##_##TimerName;

// TODO: Dont need sizes
#define LF_DEFINE_TIMER_CTOR(ReactorName, TimerName, EffectSize, ObserverSize)                                         \
  void ReactorName##_##TimerName##_ctor(ReactorName##_##TimerName *self, Reactor *parent, interval_t offset,           \
                                        interval_t period) {                                                           \
    Timer_ctor(&self->super, parent, offset, period, self->effects, EffectSize, self->observers, ObserverSize);        \
  }

#define LF_TIMER_INSTANCE(ReactorName, TimerName) ReactorName##_##TimerName TimerName;

#define LF_INITIALIZE_TIMER(ReactorName, TimerName, Offset, Period)                                                    \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->TimerName;                                                      \
  ReactorName##_##TimerName##_ctor(&self->TimerName, &self->super, Offset, Period)

#define LF_REACTION_TYPE(ReactorName, ReactionName) ReactorName##_Reaction_##ReactionName

#define LF_DEFINE_REACTION_STRUCT(ReactorName, ReactionName, EffectSize)                                               \
  typedef struct {                                                                                                     \
    Reaction super;                                                                                                    \
    Trigger *effects[(EffectSize)];                                                                                    \
  } LF_REACTION_TYPE(ReactorName, ReactionName);

#define LF_REACTION_INSTANCE(ReactorName, ReactionName) LF_REACTION_TYPE(ReactorName, ReactionName) ReactionName;

#define LF_INITIALIZE_REACTION(ReactorName, ReactionName)                                                              \
  self->_reactions[_reactions_idx++] = (Reaction *)&self->ReactionName;                                                \
  LF_REACTION_TYPE(ReactorName, ReactionName##_ctor)(&self->ReactionName, &self->super)

#define LF_DEFINE_REACTION_BODY(ReactorName, ReactionName)                                                             \
  void LF_REACTION_TYPE(ReactorName, ReactionName##_body)(Reaction * _self)

#define LF_DEFINE_REACTION_DEADLINE_VIOLATION_HANDLER(ReactorName, ReactionName)                                       \
  void LF_REACTION_TYPE(ReactorName, ReactionName##_deadline_violation_handler)(Reaction * _self)

#define LF_DEFINE_REACTION_STP_VIOLATION_HANDLER(ReactorName, ReactionName)                                            \
  void LF_REACTION_TYPE(ReactorName, ReactionName##_stp_violation_handler)(Reaction * _self)

#define LF_DEFINE_REACTION_CTOR(ReactorName, ReactionName, Priority, DeadlineHandler, Deadline, StpHandler)            \
  LF_DEFINE_REACTION_BODY(ReactorName, ReactionName);                                                                  \
  void LF_REACTION_TYPE(ReactorName, ReactionName##_ctor)(LF_REACTION_TYPE(ReactorName, ReactionName) * self,          \
                                                          Reactor * parent) {                                          \
    Reaction_ctor(&self->super, parent, LF_REACTION_TYPE(ReactorName, ReactionName##_body), self->effects,             \
                  sizeof(self->effects) / sizeof(self->effects[0]), Priority, (DeadlineHandler), (Deadline),           \
                  (StpHandler));                                                                                       \
  }

#define LF_DEFINE_STARTUP_STRUCT(ReactorName, EffectSize, ObserversSize)                                               \
  typedef struct {                                                                                                     \
    BuiltinTrigger super;                                                                                              \
    Reaction *effects[(EffectSize)];                                                                                   \
    Reaction *observers[(ObserversSize)];                                                                              \
  } ReactorName##_Startup;

#define LF_DEFINE_STARTUP_CTOR(ReactorName)                                                                            \
  void ReactorName##_Startup_ctor(ReactorName##_Startup *self, Reactor *parent) {                                      \
    BuiltinTrigger_ctor(&self->super, TRIG_STARTUP, parent, self->effects,                                             \
                        sizeof(self->effects) / sizeof(self->effects[0]), self->observers,                             \
                        sizeof(self->observers) / sizeof(self->observers[0]));                                         \
  }

#define LF_STARTUP_INSTANCE(ReactorName) ReactorName##_Startup startup;
#define LF_INITIALIZE_STARTUP(ReactorName)                                                                             \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->startup;                                                        \
  ReactorName##_Startup_ctor(&self->startup, &self->super)

#define LF_DEFINE_SHUTDOWN_STRUCT(ReactorName, EffectSize, ObserversSize)                                              \
  typedef struct {                                                                                                     \
    BuiltinTrigger super;                                                                                              \
    Reaction *effects[(EffectSize)];                                                                                   \
    Reaction *observers[(ObserversSize)];                                                                              \
  } ReactorName##_Shutdown;

#define LF_DEFINE_SHUTDOWN_CTOR(ReactorName)                                                                           \
  void ReactorName##_Shutdown_ctor(ReactorName##_Shutdown *self, Reactor *parent) {                                    \
    BuiltinTrigger_ctor(&self->super, TRIG_SHUTDOWN, parent, self->effects,                                            \
                        sizeof(self->effects) / sizeof(self->effects[0]), self->observers,                             \
                        sizeof(self->observers) / sizeof(self->observers[0]));                                         \
  }

#define LF_SHUTDOWN_INSTANCE(ReactorName) ReactorName##_Shutdown shutdown;

#define LF_INITIALIZE_SHUTDOWN(ReactorName)                                                                            \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->shutdown;                                                       \
  ReactorName##_Shutdown_ctor(&self->shutdown, &self->super)

#define LF_DEFINE_ACTION_STRUCT(ReactorName, ActionName, ActionType, EffectSize, SourceSize, ObserverSize,             \
                                MaxPendingEvents, BufferType)                                                          \
  typedef struct {                                                                                                     \
    Action super;                                                                                                      \
    BufferType value;                                                                                                  \
    BufferType payload_buf[(MaxPendingEvents)];                                                                        \
    bool payload_used_buf[(MaxPendingEvents)];                                                                         \
    Reaction *sources[(SourceSize)];                                                                                   \
    Reaction *effects[(EffectSize)];                                                                                   \
    Reaction *observers[(ObserverSize)];                                                                               \
  } ReactorName##_##ActionName;

#define LF_DEFINE_ACTION_STRUCT_VOID(ReactorName, ActionName, ActionType, EffectSize, SourceSize, ObserverSize,        \
                                     MaxPendingEvents)                                                                 \
  typedef struct {                                                                                                     \
    Action super;                                                                                                      \
    Reaction *sources[(SourceSize)];                                                                                   \
    Reaction *effects[(EffectSize)];                                                                                   \
    Reaction *observers[(ObserverSize)];                                                                               \
  } ReactorName##_##ActionName;

#define LF_DEFINE_ACTION_CTOR(ReactorName, ActionName, ActionType, EffectSize, SourceSize, ObserverSize,               \
                              MaxPendingEvents, BufferType)                                                            \
  void ReactorName##_##ActionName##_ctor(ReactorName##_##ActionName *self, Reactor *parent, interval_t min_delay,      \
                                         interval_t min_spacing) {                                                     \
    Action_ctor(&self->super, ActionType, min_delay, min_spacing, parent, self->sources, (SourceSize), self->effects,  \
                (EffectSize), self->observers, ObserverSize, &self->value, sizeof(BufferType),                         \
                (void *)&self->payload_buf, self->payload_used_buf, (MaxPendingEvents));                               \
  }

#define LF_DEFINE_ACTION_CTOR_VOID(ReactorName, ActionName, ActionType, EffectSize, SourceSize, ObserverSize,          \
                                   MaxPendingEvents)                                                                   \
  void ReactorName##_##ActionName##_ctor(ReactorName##_##ActionName *self, Reactor *parent, interval_t min_delay,      \
                                         interval_t min_spacing) {                                                     \
    Action_ctor(&self->super, ActionType, min_delay, min_spacing, parent, self->sources, (SourceSize), self->effects,  \
                (EffectSize), self->observers, ObserverSize, NULL, 0, NULL, NULL, (MaxPendingEvents));                 \
  }

#define LF_ACTION_INSTANCE(ReactorName, ActionName) ReactorName##_##ActionName ActionName;

#define LF_INITIALIZE_ACTION(ReactorName, ActionName, MinDelay, MinSpacing)                                            \
  self->_triggers[_triggers_idx++] = (Trigger *)&self->ActionName;                                                     \
  ReactorName##_##ActionName##_ctor(&self->ActionName, &self->super, MinDelay, MinSpacing)

#define LF_SCOPE_ACTION(ReactorName, ActionName)                                                                       \
  ReactorName##_##ActionName *ActionName = &self->ActionName;                                                          \
  (void)ActionName;

#define LF_SCOPE_TIMER(ReactorName, TimerName)                                                                         \
  ReactorName##_##TimerName *TimerName = &self->TimerName;                                                             \
  (void)TimerName;

#define LF_SCOPE_PORT(ReactorName, PortName)                                                                           \
  ReactorName##_##PortName *PortName = &self->PortName[0];                                                             \
  (void)PortName;

#define LF_SCOPE_MULTIPORT(ReactorName, PortName)                                                                      \
  size_t PortName##_width = sizeof(self->PortName) / sizeof(self->PortName[0]);                                        \
  ReactorName##_##PortName *PortName[PortName##_width];                                                                \
  for (int i = 0; i < PortName##_width; i++) {                                                                         \
    PortName[i] = &self->PortName[i];                                                                                  \
  }                                                                                                                    \
  (void)PortName;

#define LF_SCOPE_SELF(ReactorName)                                                                                     \
  ReactorName *self = (ReactorName *)_self->parent;                                                                    \
  (void)self;

#define LF_SCOPE_ENV()                                                                                                 \
  Environment *env = self->super.env;                                                                                  \
  (void)env;

#define LF_SCOPE_STARTUP(ReactorName)                                                                                  \
  ReactorName##_Startup *startup = &self->startup;                                                                     \
  (void)startup;

#define LF_SCOPE_SHUTDOWN(ReactorName)                                                                                 \
  ReactorName##_Shutdown *shutdown = &self->shutdown;                                                                  \
  (void)shutdown;

#define LF_DEFINE_LOGICAL_CONNECTION_STRUCT(ParentName, ConnName, DownstreamSize)                                      \
  typedef struct {                                                                                                     \
    LogicalConnection super;                                                                                           \
    Port *downstreams[(DownstreamSize)];                                                                               \
  } ParentName##_##ConnName;

#define LF_DEFINE_LOGICAL_CONNECTION_CTOR(ParentName, ConnName, DownstreamSize)                                        \
  void ParentName##_##ConnName##_ctor(ParentName##_##ConnName *self, Reactor *parent) {                                \
    LogicalConnection_ctor(&self->super, parent, self->downstreams,                                                    \
                           sizeof(self->downstreams) / sizeof(self->downstreams[0]));                                  \
  }

#define LF_LOGICAL_CONNECTION_INSTANCE(ParentName, ConnName, BankWidth, PortWidth)                                     \
  ParentName##_##ConnName ConnName[BankWidth][PortWidth];

#define LF_CHILD_OUTPUT_CONNECTIONS(ReactorName, OutputPort, BankWidth, PortWidth, NumConnsOut)                        \
  Connection *_conns_##ReactorName##_##OutputPort[BankWidth][PortWidth][NumConnsOut];

#define LF_CHILD_INPUT_SOURCES(ReactorName, InputPort, BankWidth, PortWidth, NumSources)                               \
  Reaction *_sources_##ReactorName##_##InputPort[BankWidth][PortWidth][NumSources];

#define LF_CHILD_OUTPUT_EFFECTS(ReactorName, OutputPort, BankWidth, PortWidth, NumEffects)                             \
  Reaction *_effects_##ReactorName##_##OutputPort[BankWidth][PortWidth][NumEffects];

#define LF_CHILD_OUTPUT_OBSERVERS(ReactorName, OutputPort, BankWidth, PortWidth, NumObservers)                         \
  Reaction *_observers_##ReactorName##_##OutputPort[BankWidth][PortWidth][NumObservers];

#define LF_DEFINE_CHILD_OUTPUT_ARGS(ReactorName, OutputPort, BankWidth, PortWidth)                                     \
  OutputExternalCtorArgs _##ReactorName##_##OutputPort##_args[BankWidth][PortWidth];                                   \
  for (int i = 0; i < (BankWidth); i++) {                                                                              \
    for (int j = 0; j < (PortWidth); j++) {                                                                            \
      _##ReactorName##_##OutputPort##_args[i][j].parent_effects = self->_effects_##ReactorName##_##OutputPort[i][j];   \
      _##ReactorName##_##OutputPort##_args[i][j].parent_effects_size =                                                 \
          sizeof(self->_effects_##ReactorName##_##OutputPort[i][j]) / sizeof(Reaction *);                              \
      _##ReactorName##_##OutputPort##_args[i][j].parent_observers =                                                    \
          self->_observers_##ReactorName##_##OutputPort[i][j];                                                         \
      _##ReactorName##_##OutputPort##_args[i][j].parent_observers_size =                                               \
          sizeof(self->_observers_##ReactorName##_##OutputPort[i][j]) / sizeof(Reaction *);                            \
      _##ReactorName##_##OutputPort##_args[i][j].conns_out = self->_conns_##ReactorName##_##OutputPort[i][j];          \
      _##ReactorName##_##OutputPort##_args[i][j].conns_out_size =                                                      \
          sizeof(self->_conns_##ReactorName##_##OutputPort[i][j]) / sizeof(Connection *);                              \
    }                                                                                                                  \
  }

#define LF_DEFINE_CHILD_INPUT_ARGS(ReactorName, InputPort, BankWidth, PortWidth)                                       \
  InputExternalCtorArgs _##ReactorName##_##InputPort##_args[BankWidth][PortWidth];                                     \
  for (int i = 0; i < (BankWidth); i++) {                                                                              \
    for (int j = 0; j < (PortWidth); j++) {                                                                            \
      _##ReactorName##_##InputPort##_args[i][j].parent_sources = self->_sources_##ReactorName##_##InputPort[i][j];     \
      _##ReactorName##_##InputPort##_args[i][j].parent_sources_size =                                                  \
          sizeof(self->_sources_##ReactorName##_##InputPort[i][j]) / sizeof(Reaction *);                               \
    }                                                                                                                  \
  }

#define LF_INITIALIZE_LOGICAL_CONNECTION(ParentName, ConnName, BankWidth, PortWidth)                                   \
  for (int i = 0; i < (BankWidth); i++) {                                                                              \
    for (int j = 0; j < (PortWidth); j++) {                                                                            \
      ParentName##_##ConnName##_ctor(&self->ConnName[i][j], &self->super);                                             \
    }                                                                                                                  \
  }

#define LF_DEFINE_DELAYED_CONNECTION_STRUCT(ParentName, ConnName, DownstreamSize, BufferType, BufferSize, Delay)       \
  typedef struct {                                                                                                     \
    DelayedConnection super;                                                                                           \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
    Port *downstreams[(BufferSize)];                                                                                   \
  } ParentName##_##ConnName;

#define LF_DEFINE_DELAYED_CONNECTION_CTOR(ParentName, ConnName, DownstreamSize, BufferType, BufferSize, Delay,         \
                                          IsPhysical)                                                                  \
  void ParentName##_##ConnName##_ctor(ParentName##_##ConnName *self, Reactor *parent) {                                \
    DelayedConnection_ctor(&self->super, parent, self->downstreams, DownstreamSize, Delay, IsPhysical,                 \
                           sizeof(BufferType), (void *)self->payload_buf, self->payload_used_buf, BufferSize);         \
  }
// FIXME: Duplicated
#define LF_DELAYED_CONNECTION_INSTANCE(ParentName, ConnName, BankWidth, PortWidth)                                     \
  ParentName##_##ConnName ConnName[BankWidth][PortWidth];

// FIXME: Duplicated
#define LF_INITIALIZE_DELAYED_CONNECTION(ParentName, ConnName, BankWidth, PortWidth)                                   \
  for (int i = 0; i < (BankWidth); i++) {                                                                              \
    for (int j = 0; j < (PortWidth); j++) {                                                                            \
      ParentName##_##ConnName##_ctor(&self->ConnName[i][j], &self->super);                                             \
    }                                                                                                                  \
  }

typedef struct FederatedOutputConnection FederatedOutputConnection;
#define LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(ReactorName, OutputName, BufferType)                              \
  typedef struct {                                                                                                     \
    FederatedOutputConnection super;                                                                                   \
    BufferType payload_buf[1];                                                                                         \
    bool payload_used_buf[1];                                                                                          \
  } ReactorName##_##OutputName##_conn;

#define LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(ReactorName, OutputName, BufferType, DestinationConnId)             \
  void ReactorName##_##OutputName##_conn_ctor(ReactorName##_##OutputName##_conn *self, Reactor *parent,                \
                                              FederatedConnectionBundle *bundle) {                                     \
    FederatedOutputConnection_ctor(&self->super, parent, bundle, DestinationConnId, (void *)&self->payload_buf,        \
                                   (bool *)&self->payload_used_buf, sizeof(BufferType), 1);                            \
  }

#define LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(ReactorName, OutputName) ReactorName##_##OutputName##_conn OutputName

#define LF_FEDERATED_CONNECTION_BUNDLE_TYPE(ReactorName, OtherName) ReactorName##_##OtherName##_Bundle

#define LF_FEDERATED_CONNECTION_BUNDLE_NAME(ReactorName, OtherName) ReactorName##_##OtherName##_bundle

#define LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(ReactorName, OtherName)                                                \
  LF_FEDERATED_CONNECTION_BUNDLE_TYPE(ReactorName, OtherName)                                                          \
  LF_FEDERATED_CONNECTION_BUNDLE_NAME(ReactorName, OtherName)

#define LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(ReactorName, OtherName)                                          \
  void ReactorName##_##OtherName##_Bundle_ctor(ReactorName##_##OtherName##_Bundle *self, Reactor *parent, size_t index)

#define LF_REACTOR_CTOR_SIGNATURE(ReactorName)                                                                         \
  void ReactorName##_ctor(ReactorName *self, Reactor *parent, Environment *env)

#define LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(ReactorName, ...)                                                    \
  void ReactorName##_ctor(ReactorName *self, Reactor *parent, Environment *env, __VA_ARGS__)

#define LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR()                                                                     \
  FederatedConnectionBundle_ctor(&self->super, parent, &self->channel.super, &self->inputs[0],                         \
                                 self->deserialize_hooks, sizeof(self->inputs) / sizeof(self->inputs[0]),              \
                                 &self->outputs[0], self->serialize_hooks,                                             \
                                 sizeof(self->outputs) / sizeof(self->outputs[0]), index);

#define LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(ReactorName, OtherName)                                              \
  ReactorName##_##OtherName##_Bundle_ctor(&self->ReactorName##_##OtherName##_bundle, &self->super, _bundle_idx);       \
  self->_bundles[_bundle_idx++] = &self->ReactorName##_##OtherName##_bundle.super;

#define LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(ReactorName, OutputName, SerializeFunc)                              \
  ReactorName##_##OutputName##_conn_ctor(&self->OutputName, self->super.parent, &self->super);                         \
  self->outputs[_outputs_idx] = &self->OutputName.super;                                                               \
  self->serialize_hooks[_outputs_idx] = SerializeFunc;                                                                 \
  _outputs_idx++;

typedef struct FederatedInputConnection FederatedInputConnection;
#define LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(ReactorName, InputName, BufferType, BufferSize)                    \
  typedef struct {                                                                                                     \
    FederatedInputConnection super;                                                                                    \
    BufferType payload_buf[(BufferSize)];                                                                              \
    bool payload_used_buf[(BufferSize)];                                                                               \
    Port *downstreams[1];                                                                                              \
  } ReactorName##_##InputName##_conn;

#define LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(ReactorName, InputName, BufferType, BufferSize, Delay, IsPhysical,   \
                                                  MaxWait)                                                             \
  void ReactorName##_##InputName##_conn_ctor(ReactorName##_##InputName##_conn *self, Reactor *parent) {                \
    FederatedInputConnection_ctor(&self->super, parent, Delay, IsPhysical, MaxWait, (Port **)&self->downstreams, 1,    \
                                  (void *)&self->payload_buf, (bool *)&self->payload_used_buf, sizeof(BufferType),     \
                                  BufferSize);                                                                         \
  }

#define LF_FEDERATED_INPUT_CONNECTION_INSTANCE(ReactorName, InputName) ReactorName##_##InputName##_conn InputName

#define LF_DEFINE_STARTUP_COORDINATOR_STRUCT(ReactorName, NumNeighbors)                                                \
  typedef struct {                                                                                                     \
    StartupCoordinator super;                                                                                          \
    NeighborState neighbors[NumNeighbors];                                                                             \
  } ReactorName##StartupCoordinator;

#define LF_DEFINE_STARTUP_COORDINATOR_CTOR(ReactorName, NumNeighbors, LongestPath)                                     \
  void ReactorName##StartupCoordinator_ctor(ReactorName##StartupCoordinator *self, Environment *env) {                 \
    StartupCoordinator_ctor(&self->super, env, self->neighbors, NumNeighbors, LongestPath);                            \
  }

#define LF_DEFINE_STARTUP_COORDINATOR(ReactorName) ReactorName##StartupCoordinator startup_coordinator;

#define LF_INITIALIZE_STARTUP_COORDINATOR(ReactorName)                                                                 \
  ReactorName##StartupCoordinator_ctor(&self->startup_coordinator, env);

#define LF_DEFINE_CLOCK_SYNC_STRUCT(ReactorName, NumNeighbors, NumEvents)                                              \
  typedef struct {                                                                                                     \
    ClockSynchronization super;                                                                                        \
    ClockSyncEvent events[(NumEvents)];                                                                                \
    NeighborClock neighbor_clocks[(NumNeighbors)];                                                                     \
    bool used[(NumEvents)]                                                                                             \
  } ReactorName##ClockSynchronization;

#define LF_DEFINE_CLOCK_SYNC_CTOR(ReactorName, NumNeighbors, NumEvents, IsGrandmaster)                                 \
  void ReactorName##ClockSynchronization_ctor(ReactorName##ClockSynchronization *self, Environment *env) {             \
    ClockSynchronization_ctor(&self->super, env, self->neighbor_clocks, NumNeighbors, IsGrandmaster,                   \
                              sizeof(ClockSyncEvent), (void *)self->events, self->used, (NumEvents));                  \
  }

#define LF_DEFINE_CLOCK_SYNC(ReactorName) ReactorName##ClockSynchronization clock_sync;

#define LF_INITIALIZE_CLOCK_SYNC(ReactorName) ReactorName##ClockSynchronization_ctor(&self->clock_sync, env);

#define LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(ReactorName, InputName, DeserializeFunc)                              \
  ReactorName##_##InputName##_conn_ctor(&self->InputName, self->super.parent);                                         \
  self->inputs[_inputs_idx] = &self->InputName.super;                                                                  \
  self->deserialize_hooks[_inputs_idx] = DeserializeFunc;                                                              \
  _inputs_idx++;

#define LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(NumInputs, NumOutputs)                                    \
  FederatedInputConnection *inputs[NumInputs];                                                                         \
  FederatedOutputConnection *outputs[NumOutputs];                                                                      \
  deserialize_hook deserialize_hooks[NumInputs];                                                                       \
  serialize_hook serialize_hooks[NumOutputs];

#define LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE()                                                                 \
  size_t _inputs_idx = 0;                                                                                              \
  (void)_inputs_idx;                                                                                                   \
  size_t _outputs_idx = 0;                                                                                             \
  (void)_outputs_idx;

#define LF_CHILD_REACTOR_INSTANCE(ReactorName, instanceName, BankWidth) ReactorName instanceName[BankWidth]

#define LF_INITIALIZE_CHILD_REACTOR(ReactorName, instanceName, BankWidth)                                              \
  for (int i = 0; i < BankWidth; i++) {                                                                                \
    ReactorName##_ctor(&self->instanceName[i], &self->super, env);                                                     \
    self->_children[_child_idx++] = &self->instanceName[i].super;                                                      \
  }

#define LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(ReactorName, instanceName, BankWidth, ...)                         \
  for (int i = 0; i < (BankWidth); i++) {                                                                              \
    ReactorName##_ctor(&self->instanceName[i], &self->super, env, __VA_ARGS__);                                        \
    self->_children[_child_idx++] = &self->instanceName[i].super;                                                      \
  }

#define LF_DEFINE_EVENT_QUEUE(Name, NumEvents)                                                                         \
  typedef struct {                                                                                                     \
    EventQueue super;                                                                                                  \
    ArbitraryEvent events[(NumEvents)];                                                                                \
  } Name##_t;                                                                                                          \
  static Name##_t Name;

#define LF_DEFINE_REACTION_QUEUE(Name, NumReactions)                                                                   \
  typedef struct {                                                                                                     \
    ReactionQueue super;                                                                                               \
    Reaction *reactions[(NumReactions)][(NumReactions)];                                                               \
    int level_size[(NumReactions)];                                                                                    \
  } Name##_t;                                                                                                          \
  static Name##_t Name;

#define LF_INITIALIZE_EVENT_QUEUE(Name, NumEvents) EventQueue_ctor(&Name.super, Name.events, NumEvents);

#define LF_INITIALIZE_REACTION_QUEUE(Name, NumReactions)                                                               \
  ReactionQueue_ctor(&Name.super, (Reaction **)Name.reactions, Name.level_size, NumReactions);

#define LF_ENTRY_POINT(MainReactorName, NumEvents, NumReactions, Timeout, KeepAlive, Fast)                             \
  static MainReactorName main_reactor;                                                                                 \
  static Environment env;                                                                                              \
  Environment *_lf_environment = &env;                                                                                 \
  static ArbitraryEvent events[NumEvents];                                                                             \
  static EventQueue event_queue;                                                                                       \
  static Reaction *reactions[NumReactions][NumReactions];                                                              \
  static int level_size[NumReactions];                                                                                 \
  static ReactionQueue reaction_queue;                                                                                 \
  void lf_exit(void) { Environment_free(&env); }                                                                       \
  void lf_start() {                                                                                                    \
    EventQueue_ctor(&event_queue, events, NumEvents);                                                                  \
    ReactionQueue_ctor(&reaction_queue, (Reaction **)reactions, level_size, NumReactions);                             \
    Environment_ctor(&env, (Reactor *)&main_reactor, Timeout, &event_queue, NULL, &reaction_queue, KeepAlive, false,   \
                     Fast, NULL, 0, NULL, NULL);                                                                       \
    MainReactorName##_ctor(&main_reactor, NULL, &env);                                                                 \
    env.scheduler->duration = Timeout;                                                                                 \
    env.scheduler->keep_alive = KeepAlive;                                                                             \
    env.fast_mode = Fast;                                                                                              \
    env.assemble(&env);                                                                                                \
    env.start(&env);                                                                                                   \
    lf_exit();                                                                                                         \
  }

#define LF_ENTRY_POINT_FEDERATED(FederateName, NumEvents, NumSystemEvents, NumReactions, Timeout, KeepAlive,           \
                                 NumBundles)                                                                           \
  static FederateName main_reactor;                                                                                    \
  static Environment env;                                                                                              \
  Environment *_lf_environment = &env;                                                                                 \
  static ArbitraryEvent events[(NumEvents)];                                                                           \
  static EventQueue event_queue;                                                                                       \
  static ArbitraryEvent system_events[(NumSystemEvents)];                                                              \
  static EventQueue system_event_queue;                                                                                \
  static Reaction *reactions[(NumReactions)][(NumReactions)];                                                          \
  static int level_size[(NumReactions)];                                                                               \
  static ReactionQueue reaction_queue;                                                                                 \
  void lf_exit(void) { Environment_free(&env); }                                                                       \
  void lf_start() {                                                                                                    \
    EventQueue_ctor(&event_queue, events, (NumEvents));                                                                \
    EventQueue_ctor(&system_event_queue, system_events, (NumSystemEvents));                                            \
    ReactionQueue_ctor(&reaction_queue, (Reaction **)reactions, level_size, (NumReactions));                           \
    Environment_ctor(&env, (Reactor *)&main_reactor, (Timeout), &event_queue, &system_event_queue, &reaction_queue,    \
                     (KeepAlive), true, false, (FederatedConnectionBundle **)&main_reactor._bundles, (NumBundles),     \
                     &main_reactor.startup_coordinator.super);                                                         \
    FederateName##_ctor(&main_reactor, NULL, &env);                                                                    \
    env.net_bundles_size = (NumBundles);                                                                               \
    env.net_bundles = (FederatedConnectionBundle **)&main_reactor._bundles;                                            \
    env.assemble(&env);                                                                                                \
    env.start(&env);                                                                                                   \
    lf_exit();                                                                                                         \
  }

#endif
