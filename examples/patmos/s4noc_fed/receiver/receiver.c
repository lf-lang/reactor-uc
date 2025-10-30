#include "reactor-uc/platform/patmos/s4noc_channel.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include <stdio.h>

#define SENDER_CORE_ID 2

typedef struct {
  int size;
  char msg[512];
} lf_msg_t;

lf_ret_t deserialize_msg_t(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  (void)msg_size;

  lf_msg_t *msg = user_struct;
  memcpy(&msg->size, msg_buf, sizeof(msg->size));
  memcpy(msg->msg, msg_buf + sizeof(msg->size), msg->size);

  /* Make sure msg->msg is null-terminated before printing. */
  if (msg->size < (int)sizeof(msg->msg)) {
    msg->msg[msg->size] = '\0';
  } else {
    msg->msg[sizeof(msg->msg) - 1] = '\0';
  }

  printf("Receiver: deserialize_msg_t: deserialized size=%d, msg='%s'\n", msg->size, msg->msg);

  return LF_OK;
}

LF_DEFINE_REACTION_STRUCT(Receiver, r, 0)
LF_DEFINE_REACTION_CTOR(Receiver, r, 0, NULL, NULL)
LF_DEFINE_INPUT_STRUCT(Receiver, in, 1, 0, lf_msg_t, 0)
LF_DEFINE_INPUT_CTOR(Receiver, in, 1, 0, lf_msg_t, 0)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Receiver, r);
  LF_PORT_INSTANCE(Receiver, in, 1);
  int cnt;
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 1, 0);
} Receiver;

LF_DEFINE_REACTION_BODY(Receiver, r) {
  LF_SCOPE_SELF(Receiver);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Receiver, in);
  printf("Receiver: Input triggered @ " PRINTF_TIME " with %s size %d\n", env->get_elapsed_logical_time(env), in->value.msg,
    in->value.size);
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Receiver);
  LF_INITIALIZE_REACTION(Receiver, r, NEVER);
  LF_INITIALIZE_INPUT(Receiver, in, 1, in_external);

  // Register reaction as an effect of in
  LF_PORT_REGISTER_EFFECT(self->in, self->r, 1);
}

LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(Receiver, in, lf_msg_t, 5);
LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(Receiver, in, lf_msg_t, 5, MSEC(100), false, 0);

typedef struct {
  FederatedConnectionBundle super;
  S4NOCPollChannel channel;
  LF_FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(1, 0)
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Receiver, Sender);

LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  S4NOCPollChannel_ctor(&self->channel, SENDER_CORE_ID);
  printf("Receiver: initialized channel for core %d\n", SENDER_CORE_ID);
  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}
 
typedef struct {                                                                                                     
  StartupCoordinator super;                                                                                          
  StartupEvent events[6];                                                                                  
  bool used[6];                                                                                            
  NeighborState neighbors[1];                                                                             
} ReceiverStartupCoordinator;


  void ReceiverStartupCoordinator_ctor(ReceiverStartupCoordinator *self, Environment *env) {
    StartupCoordinator_ctor(&self->super, env, self->neighbors, 1, 1, JOIN_IMMEDIATELY,
                            sizeof(StartupEvent), (void *)self->events, self->used, 6);
  }

  typedef struct {                                                                                                     
    ClockSynchronization super;
    ClockSyncEvent events[2];
    NeighborClock neighbor_clocks[1];
    bool used[2];
  } ReceiverClockSynchronization;

  void ReceiverClockSynchronization_ctor(ReceiverClockSynchronization *self, Environment *env) {
    ClockSynchronization_ctor(&self->super, env, self->neighbor_clocks, 1, false,                   
                              sizeof(ClockSyncEvent), (void *)self->events, self->used, (2),                   
                              CLOCK_SYNC_DEFAULT_PERIOD, CLOCK_SYNC_DEFAULT_MAX_ADJ, CLOCK_SYNC_DEFAULT_KP,            
                              CLOCK_SYNC_DEFAULT_KI);                                                                  
  }

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Receiver, receiver, 1);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);
  LF_FEDERATE_BOOKKEEPING_INSTANCES(1);
  LF_CHILD_INPUT_SOURCES(receiver, in, 1, 1, 0);
  LF_DEFINE_STARTUP_COORDINATOR(Receiver);
  LF_DEFINE_CLOCK_SYNC(Receiver);
} MainRecv;

LF_REACTOR_CTOR_SIGNATURE(MainRecv) {
  LF_REACTOR_CTOR(MainRecv);
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_DEFINE_CHILD_INPUT_ARGS(receiver, in, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, 1, _receiver_in_args[i]);
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  LF_INITIALIZE_STARTUP_COORDINATOR(Receiver);
  LF_INITIALIZE_CLOCK_SYNC(Receiver);
  lf_connect_federated_input(&self->Receiver_Sender_bundle.inputs[0]->super, &self->receiver->in[0].super);
}

#define NumEvents 32
#define NumSystemEvents 32
#define NumReactions 32
#define Timeout SEC(1)
#define KeepAlive true
#define NumBundles 1
#define DoClockSync false

static MainRecv main_reactor;                                                                                    
static FederatedEnvironment env;                                                                                     
Environment *_lf_environment = &env.super;                                                                           
static DynamicScheduler scheduler;                                                                                   
static ArbitraryEvent events[(NumEvents)];                                                                           
static EventQueue event_queue;                                                                                       
static ArbitraryEvent system_events[(NumSystemEvents)];                                                              
static EventQueue system_event_queue;                                                                                
static Reaction *reactions[(NumReactions)][(NumReactions)];                                                          
static int level_size[(NumReactions)];                                                                               
static ReactionQueue reaction_queue;                                                                                 

void lf_exit_receiver(void) {                                                                                                 
  printf("lf_exit_receiver: cleaning up federated environment\n");
  FederatedEnvironment_free(&env);                                                                                   
}                   
                                                                                                 
void lf_start_receiver() {                                                                                                    
  EventQueue_ctor(&event_queue, events, (NumEvents));                                                                
  EventQueue_ctor(&system_event_queue, system_events, (NumSystemEvents));                                            
  ReactionQueue_ctor(&reaction_queue, (Reaction **)reactions, level_size, (NumReactions));                           
  DynamicScheduler_ctor(&scheduler, _lf_environment, &event_queue, &system_event_queue, &reaction_queue, (Timeout),  
                        (KeepAlive));                                                                                
  FederatedEnvironment_ctor(                                                                                         
      &env, (Reactor *)&main_reactor, &scheduler.super, false, (FederatedConnectionBundle **)&main_reactor._bundles, 
      (NumBundles), &main_reactor.startup_coordinator.super, (DoClockSync) ? &main_reactor.clock_sync.super : NULL); 
  MainRecv_ctor(&main_reactor, NULL, _lf_environment);                                                         
  env.net_bundles_size = (NumBundles);                                                                               
  env.net_bundles = (FederatedConnectionBundle **)&main_reactor._bundles;                                            
  printf("lf_start_receiver: assembling federated environment (bundles=%d)\n", env.net_bundles_size);
  _lf_environment->assemble(_lf_environment);
  printf("lf_start_receiver: starting federated environment\n");
  _lf_environment->start(_lf_environment);
  printf("lf_start_receiver: federated environment started\n");
  lf_exit_receiver();                                                                                                 
}
