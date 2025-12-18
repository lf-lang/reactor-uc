#include "reactor-uc/platform/patmos/s4noc_channel.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "../common_config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

// Mutex for coordinating UART output across cores to prevent interleaving
static pthread_mutex_t uart_lock = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
   RECEIVER-SPECIFIC CONFIGURATION MACROS
   ============================================================================ */

// Sender core ID on Patmos S4NOC network
#define SENDER_CORE_ID 2
// Input pool capacity: buffer slots for incoming federated messages (CRITICAL: was 0, caused buffer overflow!)
#define INPUT_POOL_CAPACITY 5
// Timeout for waiting on federated input (milliseconds)
#define INPUT_TIMEOUT_MS 100
// Size of event queue for scheduling
#define EVENT_QUEUE_SIZE 1
// Size of system event queue (startup, timers, etc.)
#define SYSTEM_EVENT_QUEUE_SIZE 11
// Size of reaction queue for scheduling
#define REACTION_QUEUE_SIZE 1
// Timeout: shutdown receiver after this many seconds to prevent infinite execution
#define SHUTDOWN_TIMEOUT_SEC (MAX_MESSAGES * 5)

/* Derived macros */
#define NUM_EVENTS EVENT_QUEUE_SIZE
#define NUM_SYSTEM_EVENTS SYSTEM_EVENT_QUEUE_SIZE
#define NUM_REACTIONS REACTION_QUEUE_SIZE
#define TIMEOUT SEC(SHUTDOWN_TIMEOUT_SEC)
#define KEEP_ALIVE true
// Clock sync: allow physical time to elapse
#define CLOCK_SYNC_ALLOW_PHYS_TIME_ELAPSE false
// Number of input connections
#define NUM_INPUT_CONNECTIONS 1
// Number of input sources
#define NUM_INPUT_SOURCES 0

/* Message structure for federated communication */
typedef struct {
  int size;                    // Number of payload bytes (excludes size field)
  int msg_num;                 // Message sequence number
  char msg[MSG_BUF_SIZE];      // Message payload buffer
} lf_msg_t;

/* Deserialize network buffer to message structure
   Extracts message number and payload, null-terminates the string */
lf_ret_t deserialize_msg_t(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  lf_msg_t *msg = user_struct;
  
  // First 4 bytes: message number
  memcpy(&msg->msg_num, msg_buf, sizeof(int));
  
  // Remaining bytes: payload
  size_t payload_size = msg_size - sizeof(int);
  msg->size = (int)payload_size;
  
  // Copy received bytes, ensuring we don't overflow buffer
  size_t copy_len = payload_size < sizeof(msg->msg) ? payload_size : sizeof(msg->msg) - 1;
  memcpy(msg->msg, msg_buf + sizeof(int), copy_len);
  msg->msg[copy_len] = '\0';  // Null-terminate for string safety
  
  return LF_OK;
}

/* Define reactor types and constructors BEFORE using them in struct */
LF_DEFINE_REACTION_STRUCT(Receiver, r, 0)
LF_DEFINE_REACTION_CTOR(Receiver, r, 0, NULL, NULL)
LF_DEFINE_INPUT_STRUCT(Receiver, in, NUM_CHILD_REACTORS, 0, lf_msg_t, 0)
LF_DEFINE_INPUT_CTOR(Receiver, in, NUM_CHILD_REACTORS, 0, lf_msg_t, 0)

/* Receiver reactor: waits for and processes incoming federated messages */
typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Receiver, r);       // Reaction to input port
  LF_PORT_INSTANCE(Receiver, in, NUM_CHILD_REACTORS);       // Input port (federated)
  int cnt;                                  // Counter for messages received
  LF_REACTOR_BOOKKEEPING_INSTANCES(NUM_CHILD_REACTORS, 1, 0);
} Receiver;

/* Reaction to input port: process incoming federated message
   Prints the received message in bold formatting */
LF_DEFINE_REACTION_BODY(Receiver, r) {
  LF_SCOPE_SELF(Receiver);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Receiver, in);
  
  // Increment message counter
  self->cnt++;
  
  // Use mutex to prevent output interleaving with other cores
  pthread_mutex_lock(&uart_lock);
  printf("\033[1m=== RECEIVED MESSAGE %d/%d (seq #%d) ===\033[0m\n", self->cnt, MAX_MESSAGES, in->value.msg_num);
  printf("\033[1mReceiver: Input triggered @ " PRINTF_TIME " with \"%s\" size %d\033[0m\n", env->get_elapsed_logical_time(env), in->value.msg,
    in->value.size);
  printf("\033[1m========================\033[0m\n");
  
  // Request environment shutdown only after receiving all expected messages
  if (self->cnt >= MAX_MESSAGES) {
    printf("Receiver: Received all %d messages, requesting shutdown\n", MAX_MESSAGES);
    env->request_shutdown(env);
  }
  pthread_mutex_unlock(&uart_lock);
}

/* Constructor for Receiver reactor: initialize input port and reaction */
LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Receiver, InputExternalCtorArgs *in_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Receiver);
  
  // Initialize message counter
  self->cnt = 0;
  
  // Initialize reaction and federated input port
  LF_INITIALIZE_REACTION(Receiver, r, NEVER);
  LF_INITIALIZE_INPUT(Receiver, in, NUM_CHILD_REACTORS, in_external);

  // Register reaction as an effect of input port
  LF_PORT_REGISTER_EFFECT(self->in, self->r, NUM_CHILD_REACTORS);
}

// Define federated input connection for receiving from sender
LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(Receiver, in, lf_msg_t, INPUT_POOL_CAPACITY);
LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(Receiver, in, lf_msg_t, INPUT_POOL_CAPACITY, MSEC(INPUT_TIMEOUT_MS), false, CONNECTION_ID);

/* Federated connection bundle: encapsulates S4NOC channel and input connection */
typedef struct {
  FederatedConnectionBundle super;          // Base connection bundle
  S4NOCPollChannel channel;                 // S4NOC network channel
  LF_FEDERATED_INPUT_CONNECTION_INSTANCE(Receiver, in);  // Input port connection
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(NUM_INPUT_CONNECTIONS, 0)
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Receiver, Sender);

/* Constructor for federated connection bundle: set up S4NOC channel and deserialization */
LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Receiver, Sender) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  
  // Initialize S4NOC polling channel from sender core
  S4NOCPollChannel_ctor(&self->channel, SENDER_CORE_ID);
  printf("Receiver: initialized channel for core %d\n", SENDER_CORE_ID);
  
  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  // Initialize federated input connection with deserialization function
  LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(Receiver, in, deserialize_msg_t);
}
 
/* Startup coordinator: manages federated startup synchronization with sender */
typedef struct {                                                                                                     
  StartupCoordinator super;                                                                                          
  StartupEvent events[STARTUP_EVENT_SLOTS];                   // Startup event buffer
  bool used[STARTUP_EVENT_SLOTS];                             // Tracks which events are in use
  NeighborState neighbors[NUM_NEIGHBORS];               // State for neighbors
} ReceiverStartupCoordinator;


  void ReceiverStartupCoordinator_ctor(ReceiverStartupCoordinator *self, Environment *env) {
    StartupCoordinator_ctor(&self->super, env, self->neighbors, NUM_NEIGHBORS, NUM_NEIGHBORS, JOIN_IMMEDIATELY,
                            sizeof(StartupEvent), (void *)self->events, self->used, STARTUP_EVENT_SLOTS);
  }

  /* Clock synchronization: manages clock alignment with sender (if enabled) */
  typedef struct {                                                                                                     
    ClockSynchronization super;             // Base clock sync structure
    ClockSyncEvent events[CLOCK_SYNC_EVENT_SLOTS];               // Clock sync event buffer
    NeighborClock neighbor_clocks[NUM_NEIGHBOR_CLOCKS];       // Clock info for neighbors
    bool used[CLOCK_SYNC_EVENT_SLOTS];                           // Tracks which events are in use
  } ReceiverClockSynchronization;

  void ReceiverClockSynchronization_ctor(ReceiverClockSynchronization *self, Environment *env) {
    ClockSynchronization_ctor(&self->super, env, self->neighbor_clocks, NUM_NEIGHBOR_CLOCKS, CLOCK_SYNC_ALLOW_PHYS_TIME_ELAPSE,                   
                              sizeof(ClockSyncEvent), (void *)self->events, self->used, CLOCK_SYNC_EVENT_SLOTS,                   
                              CLOCK_SYNC_DEFAULT_PERIOD, CLOCK_SYNC_DEFAULT_MAX_ADJ, CLOCK_SYNC_DEFAULT_KP,            
                              CLOCK_SYNC_DEFAULT_KI);                                                                  
  }

/* Main reactor container: manages receiver reactor and federated connections */
typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Receiver, receiver, NUM_CHILD_REACTORS);  // Child receiver reactor
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Receiver, Sender);  // Federated connection to sender
  LF_FEDERATE_BOOKKEEPING_INSTANCES(NUM_BUNDLES);
  LF_CHILD_INPUT_SOURCES(receiver, in, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, NUM_INPUT_SOURCES);
  LF_DEFINE_STARTUP_COORDINATOR(Receiver);  // Startup coordination
  LF_DEFINE_CLOCK_SYNC(Receiver);            // Clock synchronization
} MainRecv;

/* Constructor for main reactor: initialize receiver and federated connections */
LF_REACTOR_CTOR_SIGNATURE(MainRecv) {
  LF_REACTOR_CTOR(MainRecv);
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_DEFINE_CHILD_INPUT_ARGS(receiver, in, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS);
  
  // Initialize child receiver reactor
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Receiver, receiver, NUM_CHILD_REACTORS, _receiver_in_args[i]);
  
  // Initialize federated connection bundle
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Receiver, Sender);
  
  // Connect federated input to receiver's input port
  lf_connect_federated_input(&self->Receiver_Sender_bundle.inputs[0]->super, &self->receiver->in[0].super);
  LF_INITIALIZE_STARTUP_COORDINATOR(Receiver);
  LF_INITIALIZE_CLOCK_SYNC(Receiver);
}

/* Global runtime state */
static MainRecv main_reactor;               // Main reactor instance
static FederatedEnvironment env;            // Federated environment
Environment *_lf_environment = &env.super;  // Environment handle
static DynamicScheduler scheduler;          // Dynamic scheduler instance

/* Event and reaction queues for scheduler */
static ArbitraryEvent events[NUM_EVENTS];    // Event queue buffer
static EventQueue event_queue;              // Event queue structure
static ArbitraryEvent system_events[NUM_SYSTEM_EVENTS];  // System event buffer
static EventQueue system_event_queue;       // System event queue structure
static Reaction *reactions[NUM_REACTIONS][NUM_REACTIONS];  // Reaction queue buffer
static int level_size[NUM_REACTIONS];        // Reaction priority levels
static ReactionQueue reaction_queue;        // Reaction queue structure

/* Cleanup function: called when receiver shuts down */
void lf_exit_receiver(void) {
  printf("lf_exit_receiver: cleaning up federated environment\n");
  FederatedEnvironment_free(&env);
}

/* Main entry point: initialize and run the receiver federated reactor */
void lf_start_receiver() {
  // Initialize event and reaction queues
  EventQueue_ctor(&event_queue, events, NUM_EVENTS);
  EventQueue_ctor(&system_event_queue, system_events, NUM_SYSTEM_EVENTS);
  ReactionQueue_ctor(&reaction_queue, (Reaction **)reactions, level_size, NUM_REACTIONS);
  
  // Create scheduler
  DynamicScheduler_ctor(&scheduler, _lf_environment, &event_queue, &system_event_queue, &reaction_queue, TIMEOUT,
                        KEEP_ALIVE);
  
  // Initialize federated environment
  FederatedEnvironment_ctor(
      &env, (Reactor *)&main_reactor, &scheduler.super, false, (FederatedConnectionBundle **)&main_reactor._bundles,
      NUM_BUNDLES, &main_reactor.startup_coordinator.super, DO_CLOCK_SYNC ? &main_reactor.clock_sync.super : NULL);
  
  // Initialize main reactor and connections
  MainRecv_ctor(&main_reactor, NULL, _lf_environment);
  env.net_bundles_size = (NUM_BUNDLES);
  env.net_bundles = (FederatedConnectionBundle **)&main_reactor._bundles;
  
  printf("lf_start_receiver: assembling federated environment (bundles=%zu)\n", env.net_bundles_size);
  _lf_environment->assemble(_lf_environment);
  
  printf("lf_start_receiver: starting federated environment\n");
  _lf_environment->start(_lf_environment);
  
  printf("lf_start_receiver: federated environment started\n");
  lf_exit_receiver();                                                                                                 
}
