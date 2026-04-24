#include "reactor-uc/platform/patmos/s4noc_channel.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environments/federated_environment.h"
#include "../common_config.h"

// Number of federated connection bundles
#define NUM_BUNDLES 2
// Startup neighbors for repeater topology: sender and receiver
#define NUM_NEIGHBORS 2
// Neighbor clocks tracked by repeater
#define NUM_NEIGHBOR_CLOCKS 2

#include <stdio.h>
#include <string.h>

/* Source sender core ID on Patmos S4NOC network */
#define SENDER_CORE_ID 3
/* Target receiver core ID on Patmos S4NOC network */
#define RECEIVER_CORE_ID 1

/* Input pool capacity for incoming federated messages */
#define INPUT_POOL_CAPACITY 5
/* Timeout for waiting on federated input */
#define INPUT_TIMEOUT_MS 100

#define EVENT_QUEUE_SIZE 2
#define SYSTEM_EVENT_QUEUE_SIZE 11
#define REACTION_QUEUE_SIZE 2
#define TIMEOUT SEC(60)
#define KEEP_ALIVE true
#define CLOCK_SYNC_ALLOW_PHYS_TIME_ELAPSE true
#define NUM_INPUT_CONNECTIONS 1
#define NUM_OUTPUT_CONNECTIONS 1
#define NUM_INPUT_SOURCES 0

typedef struct {
  int size;
  int msg_num;
  char msg[MSG_BUF_SIZE];
} lf_msg_t;

/* Serialize message structure to network buffer. */
static int serialize_msg_t(const void *user_struct, size_t user_struct_size, unsigned char *msg_buf) {
  (void)user_struct_size;
  const lf_msg_t *msg = user_struct;

  memcpy(msg_buf, &msg->msg_num, sizeof(int));
  memcpy(msg_buf + sizeof(int), msg->msg, msg->size);
  return sizeof(int) + msg->size;
}

/* Deserialize network buffer to message structure. */
static lf_ret_t deserialize_msg_t(void *user_struct, const unsigned char *msg_buf, size_t msg_size) {
  lf_msg_t *msg = user_struct;

  if (msg_size < sizeof(int)) {
    return LF_ERR;
  }

  memcpy(&msg->msg_num, msg_buf, sizeof(int));

  size_t payload_size = msg_size - sizeof(int);
  msg->size = (int)payload_size;

  size_t copy_len = payload_size < sizeof(msg->msg) ? payload_size : sizeof(msg->msg) - 1;
  memcpy(msg->msg, msg_buf + sizeof(int), copy_len);
  msg->msg[copy_len] = '\0';

  return LF_OK;
}

LF_DEFINE_REACTION_STRUCT(Repeater, r, NUM_CHILD_REACTORS)
LF_DEFINE_REACTION_CTOR(Repeater, r, 0, NULL, NULL)
LF_DEFINE_INPUT_STRUCT(Repeater, in, NUM_CHILD_REACTORS, 0, lf_msg_t, 0)
LF_DEFINE_INPUT_CTOR(Repeater, in, NUM_CHILD_REACTORS, 0, lf_msg_t, 0)
LF_DEFINE_OUTPUT_STRUCT(Repeater, out, NUM_CHILD_REACTORS, lf_msg_t)
LF_DEFINE_OUTPUT_CTOR(Repeater, out, NUM_CHILD_REACTORS)

typedef struct {
  Reactor super;
  LF_REACTION_INSTANCE(Repeater, r);
  LF_PORT_INSTANCE(Repeater, in, NUM_CHILD_REACTORS);
  LF_PORT_INSTANCE(Repeater, out, NUM_CHILD_REACTORS);
  int msg_count;
  LF_REACTOR_BOOKKEEPING_INSTANCES(NUM_CHILD_REACTORS, 2, 0);
} Repeater;

LF_DEFINE_REACTION_BODY(Repeater, r) {
  LF_SCOPE_SELF(Repeater);
  LF_SCOPE_ENV();
  LF_SCOPE_PORT(Repeater, in);
  LF_SCOPE_PORT(Repeater, out);

  self->msg_count++;

  const lf_msg_t *received = (const lf_msg_t *)&in->value;

  printf("(REPEATER) fwd seq=%d\n", received->msg_num);

  lf_msg_t val;
  val.msg_num = received->msg_num;
  val.size = received->size;
  memcpy(val.msg, received->msg, (size_t)received->size);
  lf_set(out, val);

  if (self->msg_count >= MAX_MESSAGES) {
    printf("(REPEATER) forwarded all %d message(s)\n", MAX_MESSAGES);
  }
}

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Repeater, InputExternalCtorArgs *in_external,
                                          OutputExternalCtorArgs *out_external) {
  LF_REACTOR_CTOR_PREAMBLE();
  LF_REACTOR_CTOR(Repeater);

  self->msg_count = 0;
  LF_INITIALIZE_REACTION(Repeater, r, NEVER);
  LF_INITIALIZE_INPUT(Repeater, in, NUM_CHILD_REACTORS, in_external);
  LF_INITIALIZE_OUTPUT(Repeater, out, NUM_CHILD_REACTORS, out_external);

  LF_PORT_REGISTER_EFFECT(self->in, self->r, NUM_CHILD_REACTORS);
  LF_PORT_REGISTER_SOURCE(self->out, self->r, NUM_CHILD_REACTORS);
}

LF_DEFINE_FEDERATED_INPUT_CONNECTION_STRUCT(Repeater, in, lf_msg_t, INPUT_POOL_CAPACITY)
LF_DEFINE_FEDERATED_INPUT_CONNECTION_CTOR(Repeater, in, lf_msg_t, INPUT_POOL_CAPACITY, MSEC(INPUT_TIMEOUT_MS), false,
                                          0);

LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_STRUCT(Repeater, out, lf_msg_t)
LF_DEFINE_FEDERATED_OUTPUT_CONNECTION_CTOR(Repeater, out, lf_msg_t, 0)

typedef struct {
  FederatedConnectionBundle super;
  S4NOCPollChannel channel;
  LF_FEDERATED_INPUT_CONNECTION_INSTANCE(Repeater, in);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(NUM_INPUT_CONNECTIONS, 0);
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Repeater, Sender);

LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Repeater, Sender) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();

  S4NOCPollChannel_ctor(&self->channel, SENDER_CORE_ID);

  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  LF_INITIALIZE_FEDERATED_INPUT_CONNECTION(Repeater, in, deserialize_msg_t);
}

typedef struct {
  FederatedConnectionBundle super;
  S4NOCPollChannel channel;
  LF_FEDERATED_OUTPUT_CONNECTION_INSTANCE(Repeater, out);
  LF_FEDERATED_CONNECTION_BUNDLE_BOOKKEEPING_INSTANCES(0, NUM_OUTPUT_CONNECTIONS);
} LF_FEDERATED_CONNECTION_BUNDLE_TYPE(Repeater, Receiver);

LF_FEDERATED_CONNECTION_BUNDLE_CTOR_SIGNATURE(Repeater, Receiver) {
  LF_FEDERATED_CONNECTION_BUNDLE_CTOR_PREAMBLE();

  S4NOCPollChannel_ctor(&self->channel, RECEIVER_CORE_ID);

  LF_FEDERATED_CONNECTION_BUNDLE_CALL_CTOR();
  LF_INITIALIZE_FEDERATED_OUTPUT_CONNECTION(Repeater, out, serialize_msg_t);
}

LF_DEFINE_STARTUP_COORDINATOR_STRUCT(Repeater, NUM_NEIGHBORS, STARTUP_EVENT_SLOTS);
LF_DEFINE_STARTUP_COORDINATOR_CTOR(Repeater, NUM_NEIGHBORS, NUM_NEIGHBORS, STARTUP_EVENT_SLOTS, JOIN_IMMEDIATELY);

LF_DEFINE_CLOCK_SYNC_STRUCT(Repeater, NUM_NEIGHBOR_CLOCKS, CLOCK_SYNC_EVENT_SLOTS);
LF_DEFINE_CLOCK_SYNC_DEFAULTS_CTOR(Repeater, NUM_NEIGHBOR_CLOCKS, NUM_NEIGHBOR_CLOCKS,
                                   CLOCK_SYNC_ALLOW_PHYS_TIME_ELAPSE);

typedef struct {
  Reactor super;
  LF_CHILD_REACTOR_INSTANCE(Repeater, repeater, NUM_CHILD_REACTORS);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Repeater, Sender);
  LF_FEDERATED_CONNECTION_BUNDLE_INSTANCE(Repeater, Receiver);
  LF_FEDERATE_BOOKKEEPING_INSTANCES(NUM_BUNDLES, NUM_OUTPUT_CONNECTIONS);
  LF_CHILD_INPUT_SOURCES(repeater, in, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, NUM_INPUT_SOURCES);
  LF_CHILD_OUTPUT_CONNECTIONS(repeater, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, NUM_OUTPUT_CONNECTIONS);
  LF_CHILD_OUTPUT_EFFECTS(repeater, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, 0);
  LF_CHILD_OUTPUT_OBSERVERS(repeater, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS, 0);
  LF_DEFINE_STARTUP_COORDINATOR(Repeater);
  LF_DEFINE_CLOCK_SYNC(Repeater);
} MainRepeater;

LF_REACTOR_CTOR_SIGNATURE(MainRepeater) {
  LF_REACTOR_CTOR(MainRepeater);
  LF_FEDERATE_CTOR_PREAMBLE();

  LF_DEFINE_CHILD_INPUT_ARGS(repeater, in, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS);
  LF_DEFINE_CHILD_OUTPUT_ARGS(repeater, out, NUM_CHILD_REACTORS, NUM_CHILD_REACTORS);

  LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Repeater, repeater, NUM_CHILD_REACTORS, _repeater_in_args[i],
                                              _repeater_out_args[i]);

  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Repeater, Sender);
  LF_INITIALIZE_FEDERATED_CONNECTION_BUNDLE(Repeater, Receiver);

  lf_connect_federated_input(&self->Repeater_Sender_bundle.inputs[0]->super, &self->repeater->in[0].super);
  lf_connect_federated_output((Connection *)self->Repeater_Receiver_bundle.outputs[0], (Port *)self->repeater->out);

  LF_INITIALIZE_STARTUP_COORDINATOR(Repeater);
  LF_INITIALIZE_CLOCK_SYNC(Repeater);
}

static MainRepeater main_reactor;
static FederatedEnvironment env;
Environment *_lf_environment_repeater = &env.super;

LF_DEFINE_EVENT_QUEUE(Main_EventQueue, EVENT_QUEUE_SIZE)
LF_DEFINE_EVENT_QUEUE(Main_SystemEventQueue, SYSTEM_EVENT_QUEUE_SIZE)
LF_DEFINE_REACTION_QUEUE(Main_ReactionQueue, REACTION_QUEUE_SIZE)
static DynamicScheduler _scheduler;
static Scheduler *scheduler = &_scheduler.super;

void lf_exit_repeater(void) {
  FederatedEnvironment_free(&env);
}

void lf_start_repeater(void) {
  printf("(REPEATER) stage=init\n");
  LF_INITIALIZE_EVENT_QUEUE(Main_EventQueue, EVENT_QUEUE_SIZE)
  LF_INITIALIZE_EVENT_QUEUE(Main_SystemEventQueue, SYSTEM_EVENT_QUEUE_SIZE)
  LF_INITIALIZE_REACTION_QUEUE(Main_ReactionQueue, REACTION_QUEUE_SIZE)

  DynamicScheduler_ctor(&_scheduler, _lf_environment_repeater, &Main_EventQueue.super, &Main_SystemEventQueue.super,
                        &Main_ReactionQueue.super, TIMEOUT, KEEP_ALIVE);

  FederatedEnvironment_ctor(&env, (Reactor *)&main_reactor, scheduler, false,
                            (FederatedConnectionBundle **)&main_reactor._bundles, NUM_BUNDLES,
                            &main_reactor.startup_coordinator.super,
                            (DO_CLOCK_SYNC) ? &main_reactor.clock_sync.super : NULL);

  MainRepeater_ctor(&main_reactor, NULL, _lf_environment_repeater);
  printf("(REPEATER) stage=ctor_done\n");

  printf("(REPEATER) stage=assemble\n");
  _lf_environment_repeater->assemble(_lf_environment_repeater);

  if (scheduler->start_time == NEVER) {
    instant_t t0 = _lf_environment_repeater->get_physical_time(_lf_environment_repeater);
    tag_t start_tag = {.time = t0, .microstep = 0};
    printf("(REPEATER) fallback_start_tag @ " PRINTF_TIME "\n", t0);
    scheduler->prepare_timestep(scheduler, NEVER_TAG);
    Environment_schedule_startups(_lf_environment_repeater, start_tag);
    Environment_schedule_timers(_lf_environment_repeater, _lf_environment_repeater->main, start_tag);
    scheduler->prepare_timestep(scheduler, start_tag);
    scheduler->set_and_schedule_start_tag(scheduler, t0);
  }

  printf("(REPEATER) stage=start\n");
  _lf_environment_repeater->start(_lf_environment_repeater);

  printf("(REPEATER) stage=done\n");
  lf_exit_repeater();
}
