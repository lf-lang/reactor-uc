#include "reactor-uc/platform/patmos/s4noc_channel.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environments/federated_environment.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

static pthread_mutex_t uart_lock = PTHREAD_MUTEX_INITIALIZER;

 #define RECEIVER_CORE_ID 1

#define DoClockSync false

/* Configurable literal macros */
#ifndef MESSAGE_TEXT
#define MESSAGE_TEXT "Hello"
#endif
// Length without the terminating null; we send exactly this many bytes on the wire.
#define MESSAGE_TEXT_LEN ((int)strlen(MESSAGE_TEXT))
#define MSG_BUF_SIZE 512
#define TIMER_START_SEC 1
#define TIMER_PERIOD_SEC 1
#define EVENT_QUEUE_SIZE 1
#define SYSTEM_EVENT_QUEUE_SIZE 11
#define REACTION_QUEUE_SIZE 1
#define MAX_MESSAGES 1  // Send only 1 message then stop

typedef struct {
  int size;           // Number of payload bytes (excludes size field)
  char msg[MSG_BUF_SIZE];
} lf_msg_t;

int serialize_msg_t(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  (void)user_struct_size;
  const lf_msg_t *msg = user_struct;

  // Send only the payload bytes; the tagged message already carries the length.
  memcpy(msg_buf, msg->msg, msg->size);
  return msg->size;
}

LF_DEFINE_TIMER_STRUCT(Sender, t, 1, 0)
LF_DEFINE_TIMER_CTOR(Sender, t, 1, 0)
LF_DEFINE_REACTION_STRUCT(Sender, r, 1)
LF_DEFINE_REACTION_CTOR(Sender, r, 0, NULL, NULL)
LF_DEFINE_OUTPUT_STRUCT(Sender, out, 1, lf_msg_t)
LF_DEFINE_OUTPUT_CTOR(Sender, out, 1)

typedef struct {
  Reactor super;
  LF_TIMER_INSTANCE(Sender, t);
  LF_REACTION_INSTANCE(Sender, r);
  LF_PORT_INSTANCE(Sender, out, 1);
  int msg_count;  // Track number of messages sent
  LF_REACTOR_BOOKKEEPING_INSTANCES(1, 2, 0);
} Sender;

LF_DEFINE_REACTION_BODY(Sender, r) {
  LF_SCOPE_SELF(Sender);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Sender, out);

  // Only send message if we haven't reached MAX_MESSAGES yet
  if (self->msg_count >= MAX_MESSAGES) {
    pthread_mutex_lock(&uart_lock);
    printf("Sender: Already sent %d message(s). Not sending more.\n", self->msg_count);
    pthread_mutex_unlock(&uart_lock);
    return;
  }

  pthread_mutex_lock(&uart_lock);
  printf("Sender: Timer triggered @ " PRINTF_TIME "\n", env->get_elapsed_logical_time(env));
  lf_msg_t val;
  memcpy(val.msg, MESSAGE_TEXT, MESSAGE_TEXT_LEN);
  val.msg[MESSAGE_TEXT_LEN] = '\0';
  val.size = MESSAGE_TEXT_LEN;
  printf("Sender reaction: preparing message size=%d, msg='%s'\n", val.size, val.msg);
  lf_set(out, val);
  
  // Increment message counter
  self->msg_count++;
  printf("Sender: Message %d sent. Max messages: %d\n", self->msg_count, MAX_MESSAGES);
  pthread_mutex_unlock(&uart_lock);
  
  env->request_shutdown(env);
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Sender, OutputExternalCtorArgs *out_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Sender);
  printf("Sender: Initializing reaction and timer...\n");
  self->msg_count = 0;  // Initialize message counter
  LF_INITIALIZE_REACTION(Sender, r, NEVER);
  LF_INITIALIZE_TIMER(Sender, t, SEC(TIMER_START_SEC), SEC(TIMER_PERIOD_SEC));  
  LF_INITIALIZE_OUTPUT(Sender, out, 1, out_external);

  printf("Sender: Registering timer effects and port sources...\n");
  LF_TIMER_REGISTER_EFFECT(self->t, self->r);
  LF_PORT_REGISTER_SOURCE(self->out, self->r, 1);
}

LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(Sender, out, lf_msg_t)
LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(Sender, out, lf_msg_t, 0)

typedef struct {
  FederatedConnectionBundle super;
  S4NOCPollChannel channel;
  LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(Sender, out);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0, 1);
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Sender, Receiver);

LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Sender, Receiver) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();
  S4NOCPollChannel_ctor(&self->channel, RECEIVER_CORE_ID);
  printf("Sender: initialized channel for core %d\n", RECEIVER_CORE_ID);
  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Sender, out, serialize_msg_t);
}

LF_DEFINE_STARTUP_COORDINATOR_STRUCT(Sender, 1, 6);
LF_DEFINE_STARTUP_COORDINATOR_CTOR(Sender, 1, 1, 6, JOIN_IMMEDIATELY);

LF_DEFINE_CLOCK_SYNC_STRUCT(Sender, 1, 2);
LF_DEFINE_CLOCK_SYNC_DEFAULTS_CTOR(Sender, 1, 1, true);

// Reactor main
typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Sender, sender, 1);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Sender, Receiver);
  LF_FEDERATE_BOOKKEEPING_INSTANCES(1);
  LF_CHILD_OUTPUT_CONNECTIONS(sender, out, 1, 1, 1);
  LF_CHILD_OUTPUT_EFFECTS(sender, out, 1, 1, 0);
  LF_CHILD_OUTPUT_OBSERVERS(sender, out, 1, 1, 0);
  LF_DEFINE_STARTUP_COORDINATOR(Sender);
  LF_DEFINE_CLOCK_SYNC(Sender);
} MainSender;

LF_REACTOR_CTOR_SIGNATURE(MainSender) {
  LF_REACTOR_CTOR(MainSender);
  LF_FEDERATE_CTOR_PREAMBLE();
  LF_DEFINE_CHILD_OUTPUT_ARGS(sender, out, 1, 1);
  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Sender, sender, 1, _sender_out_args[i]);
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Sender, Receiver);
  lf_connect_federated_output((Connection *)self->Sender_Receiver_bundle.outputs[0], (Port *)self->sender->out);
  LF_INITIALIZE_STARTUP_COORDINATOR(Sender);
  LF_INITIALIZE_CLOCK_SYNC(Sender);
}

static MainSender main_reactor;
static FederatedEnvironment env;
Environment *_lf_environment_sender = &env.super;
// Define queues used by scheduler
LF_DEFINE_EVENT_QUEUE(Main_EventQueue, EVENT_QUEUE_SIZE)
LF_DEFINE_EVENT_QUEUE(Main_SystemEventQueue, SYSTEM_EVENT_QUEUE_SIZE)
LF_DEFINE_REACTION_QUEUE(Main_ReactionQueue, REACTION_QUEUE_SIZE)
static DynamicScheduler _scheduler;
static Scheduler* scheduler = &_scheduler.super;

void lf_exit_sender(void) {
  printf("lf_exit_sender: cleaning up federated environment\n");
  FederatedEnvironment_free(&env);
}

void lf_start_sender(void) {
  // Define queues used by scheduler
  LF_INITIALIZE_EVENT_QUEUE(Main_EventQueue, EVENT_QUEUE_SIZE)
  LF_INITIALIZE_EVENT_QUEUE(Main_SystemEventQueue, SYSTEM_EVENT_QUEUE_SIZE)
  LF_INITIALIZE_REACTION_QUEUE(Main_ReactionQueue, REACTION_QUEUE_SIZE)
  // keep_alive=true so sender keeps running; duration FOREVER
  DynamicScheduler_ctor(&_scheduler, _lf_environment_sender, &Main_EventQueue.super, &Main_SystemEventQueue.super, &Main_ReactionQueue.super, FOREVER, true);
  FederatedEnvironment_ctor(&env, (Reactor *)&main_reactor, scheduler, false,  
                    (FederatedConnectionBundle **) &main_reactor._bundles, 1, &main_reactor.startup_coordinator.super, 
                    (DoClockSync) ? &main_reactor.clock_sync.super : NULL);
  MainSender_ctor(&main_reactor, NULL, _lf_environment_sender);
  printf("lf_start_sender: assembling federated environment (bundles=%zu)\n", env.net_bundles_size);
  _lf_environment_sender->assemble(_lf_environment_sender);
  printf("lf_start_sender: starting federated environment\n");
  _lf_environment_sender->start(_lf_environment_sender);
  printf("lf_start_sender: federated environment started\n");
  lf_exit_sender();
}
