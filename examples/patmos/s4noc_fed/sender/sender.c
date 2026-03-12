#include "reactor-uc/platform/patmos/s4noc_channel.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environments/federated_environment.h"
#include "../common_config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

// Mutex for coordinating UART output across cores to prevent interleaving
static pthread_mutex_t uart_lock = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
   SENDER-SPECIFIC CONFIGURATION MACROS
   ============================================================================ */

// Target receiver core ID on Patmos S4NOC network
#define RECEIVER_CORE_ID 1

/* Configurable literal macros for message and timing */
#ifndef MESSAGE_TEXT
#define MESSAGE_TEXT "Hello"
#endif
// Message text length (excludes null terminator; only these bytes are sent on network)
#define MESSAGE_TEXT_LEN ((int)strlen(MESSAGE_TEXT))
// Timer offset: start firing after 1 second of simulation time
#define TIMER_START_SEC 1
// Timer period: fire every TIMER_PERIOD_SEC seconds
#define TIMER_PERIOD_SEC 2
// Timeout: shutdown sender after this many seconds to prevent infinite execution
#define SHUTDOWN_TIMEOUT_SEC (TIMER_START_SEC + (MAX_MESSAGES * TIMER_PERIOD_SEC) + 5)
// Size of event queue for scheduling
#define EVENT_QUEUE_SIZE 1
// Size of system event queue (startup, timers, etc.)
#define SYSTEM_EVENT_QUEUE_SIZE 11
// Size of reaction queue for scheduling
#define REACTION_QUEUE_SIZE 1
// Scheduler timeout duration
#define TIMEOUT SEC(SHUTDOWN_TIMEOUT_SEC)
// Keep scheduler alive
#define KEEP_ALIVE true
// Clock sync: allow physical time to elapse
#define CLOCK_SYNC_ALLOW_PHYS_TIME_ELAPSE true
// Number of output connections
#define NUM_OUTPUT_CONNECTIONS 1

typedef struct {
  int size;                    // Number of payload bytes (excludes size field)
  int msg_num;                 // Message sequence number
  char msg[MSG_BUF_SIZE];      // Message payload buffer
} lf_msg_t;

/* Serialize message structure to network buffer
   Sends message number followed by payload bytes */
int serialize_msg_t(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  (void)user_struct_size;
  const lf_msg_t *msg = user_struct;

  // First 4 bytes: message number
  memcpy(msg_buf, &msg->msg_num, sizeof(int));
  // Followed by payload bytes
  memcpy(msg_buf + sizeof(int), msg->msg, msg->size);
  return sizeof(int) + msg->size;  // Return total bytes sent
}

LF_DEFINE_TIMER_STRUCT(Sender, t, NUM_CHILD_REACTORS, 0)
LF_DEFINE_TIMER_CTOR(Sender, t, NUM_CHILD_REACTORS, 0)
LF_DEFINE_REACTION_STRUCT(Sender, r, NUM_CHILD_REACTORS)
LF_DEFINE_REACTION_CTOR(Sender, r, 0, NULL, NULL)
LF_DEFINE_OUTPUT_STRUCT(Sender, out, NUM_CHILD_REACTORS, lf_msg_t)
LF_DEFINE_OUTPUT_CTOR(Sender, out, NUM_CHILD_REACTORS)

/* Sender reactor: periodically sends "Hello" message to receiver */
typedef struct {
  Reactor super;
  LF_TIMER_INSTANCE(Sender, t);           // Timer trigger
  LF_REACTION_INSTANCE(Sender, r);        // Reaction to timer
  LF_PORT_INSTANCE(Sender, out, NUM_CHILD_REACTORS);       // Output port (federated)
  int msg_count;                          // Tracks number of messages sent
  LF_REACTOR_BOOKKEEPING_INSTANCES(NUM_CHILD_REACTORS, 2, 0);
} Sender;

/* Reaction to timer: send one message then shutdown
   This reaction fires when the timer expires */
LF_DEFINE_REACTION_BODY(Sender, r) {
  LF_SCOPE_SELF(Sender);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Sender, out);

  // Check if we've already sent maximum messages
  if (self->msg_count >= MAX_MESSAGES) {
    pthread_mutex_lock(&uart_lock);
    printf("Sender: Already sent %d message(s). Not sending more.\n", self->msg_count);
    pthread_mutex_unlock(&uart_lock);
    return;
  }

  pthread_mutex_lock(&uart_lock);
  printf("Sender: Timer triggered @ " PRINTF_TIME "\n", env->get_elapsed_logical_time(env));
  
  // Prepare message structure
  lf_msg_t val;
  val.msg_num = self->msg_count + 1;  // Message number (1-indexed)
  
  // Create message with number: "Hello 1", "Hello 2", etc.
  snprintf(val.msg, MSG_BUF_SIZE, "%s %d", MESSAGE_TEXT, val.msg_num);
  val.size = (int)strlen(val.msg);
  
  printf("Sender reaction: preparing message #%d, size=%d, msg='%s'\n", val.msg_num, val.size, val.msg);
  
  // Send message to receiver via federated output port
  lf_set(out, val);
  
  // Increment and log message counter
  self->msg_count++;
  printf("Sender: Message %d sent. Max messages: %d\n", self->msg_count, MAX_MESSAGES);
  pthread_mutex_unlock(&uart_lock);
  
  // Request environment shutdown only after sending all messages
  // Wait one more timer period to ensure last message is fully transmitted
  if (self->msg_count > MAX_MESSAGES) {
    pthread_mutex_lock(&uart_lock);
    printf("Sender: Transmission complete, requesting shutdown\n");
    pthread_mutex_unlock(&uart_lock);
    env->request_shutdown(env);
  }
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs *out_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Sender);
  printf("Sender: Initializing reaction and timer...\n");
  self->msg_count = 0;  // Initialize message counter
  LF_INITIALIZE_REACTION(Sender, r, NEVER);
  LF_INITIALIZE_TIMER(Sender, t, SEC(TIMER_START_SEC), SEC(TIMER_PERIOD_SEC));  
  LF_INITIALIZE_OUTPUT(Sender, out, NUM_CHILD_REACTORS, out_external);

  printf("Sender: Registering timer effects and port sources...\n");
  LF_TIMER_REGISTER_EFFECT(self->t, self->r);
  LF_PORT_REGISTER_SOURCE(self->out, self->r, NUM_CHILD_REACTORS);
}

LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(Sender, out, lf_msg_t)
LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(Sender, out, lf_msg_t, CONNECTION_ID)

/* Federated connection bundle: encapsulates S4NOC channel and output connection */
typedef struct {
  FederatedConnectionBundle super;         // Base connection bundle
  S4NOCPollChannel channel;                // S4NOC network channel
  LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);  // Output port connection
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0, NUM_OUTPUT_CONNECTIONS);
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Sender, Receiver);

/* Constructor for federated connection bundle: set up S4NOC channel and serialization */
LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  
  // Initialize S4NOC polling channel to receiver core
  S4NOCPollChannel_ctor(&self->channel, RECEIVER_CORE_ID);
  printf("Sender: initialized channel for core %d\n", RECEIVER_CORE_ID);
  
  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  // Initialize federated output connection with serialization function
  LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);
}

LF_DEFINE_STARTUP_COORDINATOR_STRUCT(Sender, NUM_NEIGHBORS, STARTUP_EVENT_SLOTS);
LF_DEFINE_STARTUP_COORDINATOR_CTOR(Sender, NUM_NEIGHBORS, NUM_NEIGHBORS, STARTUP_EVENT_SLOTS, JOIN_IMMEDIATELY);

LF_DEFINE_CLOCK_SYNC_STRUCT(Sender, NUM_NEIGHBOR_CLOCKS, CLOCK_SYNC_EVENT_SLOTS);
LF_DEFINE_CLOCK_SYNC_DEFAULTS_CTOR(Sender, NUM_NEIGHBOR_CLOCKS, NUM_NEIGHBOR_CLOCKS, CLOCK_SYNC_ALLOW_PHYS_TIME_ELAPSE);

/* Main reactor container: manages sender reactor and federated connections */
typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Sender, sender, NUM_CHILD_REACTORS);  // Child sender reactor
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver);  // Federated connection to receiver
  LF_FEDERATE_BOOKKEEPING_INSTANCES(NUM_BUNDLES);
  LF_CHILD_OUTPUT_CONNECTIONS(sender, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, NUM_OUTPUT_CONNECTIONS);
  LF_CHILD_OUTPUT_EFFECTS(sender, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, 0);
  LF_CHILD_OUTPUT_OBSERVERS(sender, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, 0);
  LF_DEFINE_STARTUP_COORDINATOR(Sender);  // Startup coordination
  LF_DEFINE_CLOCK_SYNC(Sender);             // Clock synchronization
} MainSender;

/* Constructor for main reactor: initialize sender and federated connections */
LF_REACTOR_CTOR_SIGNATURE(MainSender) {
  LF_REACTOR_CTOR(MainSender);
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_DEFINE_CHILD_OUTPUT_ARGS(sender, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS);
  
  // Initialize child sender reactor
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, NUM_CHILD_REACTORS, _sender_out_args[i]);
  
  // Initialize federated connection bundle
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver);
  
  // Connect sender's output port to federated connection
  lf_connect_federated_output((Connection *)self->Sender_Receiver_bundle.outputs[0], (Port *)self->sender->out);
  
  LF_INITIALIZE_STARTUP_COORDINATOR(Sender);
  LF_INITIALIZE_CLOCK_SYNC(Sender);
}

/* Global runtime state */
static MainSender main_reactor;              // Main reactor instance
static FederatedEnvironment env;             // Federated environment
Environment *_lf_environment_sender = &env.super;  // Environment handle

/* Event and reaction queues for scheduler */
LF_DEFINE_EVENT_QUEUE(Main_EventQueue, EVENT_QUEUE_SIZE)
LF_DEFINE_EVENT_QUEUE(Main_SystemEventQueue, SYSTEM_EVENT_QUEUE_SIZE)
LF_DEFINE_REACTION_QUEUE(Main_ReactionQueue, REACTION_QUEUE_SIZE)
static DynamicScheduler _scheduler;         // Dynamic scheduler instance
static Scheduler* scheduler = &_scheduler.super;

/* Cleanup function: called when sender shuts down */
void lf_exit_sender(void) {
  printf("lf_exit_sender: cleaning up federated environment\n");
  FederatedEnvironment_free(&env);
}

/* Main entry point: initialize and run the sender federated reactor */
void lf_start_sender(void) {
  // Initialize event and reaction queues
  LF_INITIALIZE_EVENT_QUEUE(Main_EventQueue, EVENT_QUEUE_SIZE)
  LF_INITIALIZE_EVENT_QUEUE(Main_SystemEventQueue, SYSTEM_EVENT_QUEUE_SIZE)
  LF_INITIALIZE_REACTION_QUEUE(Main_ReactionQueue, REACTION_QUEUE_SIZE)
  
  // Create scheduler
  DynamicScheduler_ctor(&_scheduler, _lf_environment_sender, &Main_EventQueue.super, &Main_SystemEventQueue.super, &Main_ReactionQueue.super, TIMEOUT, KEEP_ALIVE);
  
  // Initialize federated environment
  FederatedEnvironment_ctor(&env, (Reactor *)&main_reactor, scheduler, false,  
                    (FederatedConnectionBundle **) &main_reactor._bundles, NUM_BUNDLES, &main_reactor.startup_coordinator.super, 
                    (DO_CLOCK_SYNC) ? &main_reactor.clock_sync.super : NULL);
  
  // Initialize main reactor and connections
  MainSender_ctor(&main_reactor, NULL, _lf_environment_sender);
  
  printf("lf_start_sender: assembling federated environment (bundles=%zu)\n", env.net_bundles_size);
  _lf_environment_sender->assemble(_lf_environment_sender);
  
  printf("lf_start_sender: starting federated environment\n");
  _lf_environment_sender->start(_lf_environment_sender);
  
  printf("lf_start_sender: federated environment started\n");
  lf_exit_sender();
}
