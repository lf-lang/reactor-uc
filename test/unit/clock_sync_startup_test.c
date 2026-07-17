#include <string.h>

#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environments/federated_environment.h"
#include "reactor-uc/schedulers/dynamic/scheduler.h"
#include "reactor-uc/startup_coordinator.h"
#include "unity.h"

#define QUEUE_CAPACITY 16
#define PAYLOAD_CAPACITY 8

typedef struct {
  Platform super;
  size_t notify_count;
} FakePlatform;

typedef struct {
  NetworkChannel super;
  lf_ret_t send_result;
  size_t send_count;
  FederateMessage last_message;
} FakeNetworkChannel;

typedef struct {
  FederatedEnvironment env;
  FakePlatform platform;
  DynamicScheduler scheduler;
  EventQueue event_queue;
  ArbitraryEvent event_storage[QUEUE_CAPACITY];
  EventQueue system_event_queue;
  ArbitraryEvent system_event_storage[QUEUE_CAPACITY];
  ReactionQueue reaction_queue;
  Reaction* reaction_storage[1];
  int reaction_level_size[1];
  FakeNetworkChannel channel;
  FederatedConnectionBundle bundle;
  FederatedConnectionBundle* bundles[1];
  StartupCoordinator startup;
  NeighborState neighbor_state[1];
  StartupEvent startup_payloads[PAYLOAD_CAPACITY];
  bool startup_payloads_used[PAYLOAD_CAPACITY];
  ClockSynchronization clock_sync;
  NeighborClock neighbor_clock[1];
  ClockSyncEvent clock_payloads[PAYLOAD_CAPACITY];
  bool clock_payloads_used[PAYLOAD_CAPACITY];
} ClockSyncStartupFixture;

static lf_ret_t fake_clock_set_result;
static lf_ret_t fake_clock_adjust_result;

static void fake_platform_notify(Platform* platform) { ((FakePlatform*)platform)->notify_count++; }

static lf_ret_t fake_channel_send(NetworkChannel* channel, const FederateMessage* message) {
  FakeNetworkChannel* fake = (FakeNetworkChannel*)channel;
  fake->send_count++;
  fake->last_message = *message;
  return fake->send_result;
}

static instant_t fake_clock_get_time(PhysicalClock* clock) { return clock->offset; }

static lf_ret_t fake_clock_set_time(PhysicalClock* clock, instant_t time) {
  if (fake_clock_set_result == LF_OK) {
    clock->offset = time;
  }
  return fake_clock_set_result;
}

static lf_ret_t fake_clock_adjust_time(PhysicalClock* clock, interval_t adjustment) {
  (void)clock;
  (void)adjustment;
  return fake_clock_adjust_result;
}

static instant_t fake_environment_get_physical_time(Environment* env) {
  return ((FederatedEnvironment*)env)->clock.get_time(&((FederatedEnvironment*)env)->clock);
}

static void fixture_ctor(ClockSyncStartupFixture* fixture, bool is_grandmaster) {
  memset(fixture, 0, sizeof(*fixture));
  fake_clock_set_result = LF_OK;
  fake_clock_adjust_result = LF_OK;

  fixture->platform.super.notify = fake_platform_notify;
  fixture->channel.super.send_blocking = fake_channel_send;
  fixture->channel.send_result = LF_OK;
  fixture->bundle.net_channel = &fixture->channel.super;
  fixture->bundles[0] = &fixture->bundle;

  EventQueue_ctor(&fixture->event_queue, fixture->event_storage, QUEUE_CAPACITY);
  EventQueue_ctor(&fixture->system_event_queue, fixture->system_event_storage, QUEUE_CAPACITY);
  ReactionQueue_ctor(&fixture->reaction_queue, fixture->reaction_storage, fixture->reaction_level_size, 1);

  fixture->env.super.platform = &fixture->platform.super;
  fixture->env.super.get_physical_time = fake_environment_get_physical_time;
  fixture->env.net_bundles = fixture->bundles;
  fixture->env.net_bundles_size = 1;
  fixture->env.do_clock_sync = true;
  fixture->env.clock.get_time = fake_clock_get_time;
  fixture->env.clock.set_time = fake_clock_set_time;
  fixture->env.clock.adjust_time = fake_clock_adjust_time;
  fixture->env.clock.offset = SEC(1);

  DynamicScheduler_ctor(&fixture->scheduler, &fixture->env.super, &fixture->event_queue,
                        &fixture->system_event_queue, &fixture->reaction_queue, FOREVER, false);
  fixture->env.super.scheduler = &fixture->scheduler.super;

  StartupCoordinator_ctor(&fixture->startup, &fixture->env.super, fixture->neighbor_state, 1, 1, JOIN_IMMEDIATELY,
                          sizeof(StartupEvent), fixture->startup_payloads, fixture->startup_payloads_used,
                          PAYLOAD_CAPACITY);
  fixture->env.startup_coordinator = &fixture->startup;

  fixture->env.clock_sync = &fixture->clock_sync;
  ClockSynchronization_ctor(&fixture->clock_sync, &fixture->env.super, fixture->neighbor_clock, 1, is_grandmaster,
                            sizeof(ClockSyncEvent), fixture->clock_payloads, fixture->clock_payloads_used,
                            PAYLOAD_CAPACITY, SEC(1), 200000000, 0.7f, 0.3f);
}

static void deliver_startup_message(ClockSyncStartupFixture* fixture, int neighbor_index,
                                    const StartupCoordination* message) {
  StartupEvent* payload = NULL;
  TEST_ASSERT_EQUAL(LF_OK, fixture->startup.super.payload_pool.allocate(&fixture->startup.super.payload_pool,
                                                                       (void**)&payload));
  payload->neighbor_index = neighbor_index;
  payload->msg = *message;
  SystemEvent event = SYSTEM_EVENT_INIT(((tag_t){.time = fixture->env.clock.offset}), &fixture->startup.super, payload);
  fixture->startup.super.handle(&fixture->startup.super, &event);
}

static void deliver_clock_message(ClockSyncStartupFixture* fixture, int neighbor_index,
                                  const ClockSyncMessage* message) {
  ClockSyncEvent* payload = NULL;
  TEST_ASSERT_EQUAL(LF_OK, fixture->clock_sync.super.payload_pool.allocate(&fixture->clock_sync.super.payload_pool,
                                                                          (void**)&payload));
  payload->neighbor_index = neighbor_index;
  payload->msg = *message;
  SystemEvent event =
      SYSTEM_EVENT_INIT(((tag_t){.time = fixture->env.clock.offset}), &fixture->clock_sync.super, payload);
  fixture->clock_sync.super.handle(&fixture->clock_sync.super, &event);
}

static void complete_startup_handshake(ClockSyncStartupFixture* fixture) {
  fixture->startup.state = StartupCoordinationState_HANDSHAKING;
  StartupCoordination handshake = StartupCoordination_init_zero;
  handshake.which_message = StartupCoordination_startup_handshake_response_tag;
  handshake.message.startup_handshake_response.state = StartupCoordinationState_HANDSHAKING;
  deliver_startup_message(fixture, 0, &handshake);
}

static void deliver_initial_large_offset(ClockSyncStartupFixture* fixture) {
  const instant_t epoch_time = SEC(1700000000LL);
  fixture->clock_sync.sequence_number = 7;
  fixture->clock_sync.timestamps.t1 = epoch_time;
  fixture->clock_sync.timestamps.t2 = SEC(1);
  fixture->clock_sync.timestamps.t3 = SEC(1) + 100;

  ClockSyncMessage response = ClockSyncMessage_init_zero;
  response.which_message = ClockSyncMessage_delay_response_tag;
  response.message.delay_response.sequence_number = 7;
  response.message.delay_response.time = epoch_time + 100;
  deliver_clock_message(fixture, 0, &response);
}

static void deliver_initial_small_offset(ClockSyncStartupFixture* fixture) {
  fixture->clock_sync.sequence_number = 8;
  fixture->clock_sync.timestamps.t1 = SEC(1) + 50;
  fixture->clock_sync.timestamps.t2 = SEC(1);
  fixture->clock_sync.timestamps.t3 = SEC(1) + 100;

  ClockSyncMessage response = ClockSyncMessage_init_zero;
  response.which_message = ClockSyncMessage_delay_response_tag;
  response.message.delay_response.sequence_number = 8;
  response.message.delay_response.time = SEC(1) + 150;
  deliver_clock_message(fixture, 0, &response);
}

static size_t count_startup_system_events(const ClockSyncStartupFixture* fixture, int message_type) {
  size_t count = 0;
  for (size_t i = 0; i < fixture->system_event_queue.size; i++) {
    const SystemEvent* event = &fixture->system_event_queue.array[i].system_event;
    if (event->handler == &fixture->startup.super &&
        ((const StartupEvent*)event->super.payload)->msg.which_message == message_type) {
      count++;
    }
  }
  return count;
}

void test_clock_sync_constructor_initializes_readiness(void) {
  ClockSyncStartupFixture grandmaster;
  ClockSyncStartupFixture follower;
  fixture_ctor(&grandmaster, true);
  fixture_ctor(&follower, false);

  TEST_ASSERT_TRUE(grandmaster.clock_sync.has_initial_sync);
  TEST_ASSERT_FALSE(follower.clock_sync.has_initial_sync);
}

void test_non_grandmaster_handshake_does_not_schedule_application_startup_before_sync(void) {
  ClockSyncStartupFixture fixture;
  fixture_ctor(&fixture, false);
  complete_startup_handshake(&fixture);

  TEST_ASSERT_EQUAL(StartupCoordinationState_NEGOTIATING, fixture.startup.state);
  TEST_ASSERT_EQUAL_UINT(0,
                         count_startup_system_events(&fixture, StartupCoordination_start_time_proposal_tag));
  TEST_ASSERT_EQUAL_UINT(0, fixture.event_queue.size);

  StartupCoordination early_proposal = StartupCoordination_init_zero;
  early_proposal.which_message = StartupCoordination_start_time_proposal_tag;
  early_proposal.message.start_time_proposal.time = SEC(5);
  early_proposal.message.start_time_proposal.step = 1;
  deliver_startup_message(&fixture, 0, &early_proposal);

  TEST_ASSERT_EQUAL_UINT(0, fixture.event_queue.size);
}

void test_grandmaster_preserves_immediate_startup_negotiation(void) {
  ClockSyncStartupFixture fixture;
  fixture_ctor(&fixture, true);
  complete_startup_handshake(&fixture);

  TEST_ASSERT_TRUE(fixture.startup.start_time_proposal_scheduled);
  TEST_ASSERT_EQUAL_UINT(1,
                         count_startup_system_events(&fixture, StartupCoordination_start_time_proposal_tag));
}

void test_successful_initial_clock_step_releases_startup_gate_once(void) {
  ClockSyncStartupFixture fixture;
  fixture_ctor(&fixture, false);
  complete_startup_handshake(&fixture);
  TEST_ASSERT_FALSE(fixture.startup.start_time_proposal_scheduled);

  deliver_initial_large_offset(&fixture);

  TEST_ASSERT_TRUE(fixture.clock_sync.has_initial_sync);
  TEST_ASSERT_TRUE(fixture.startup.start_time_proposal_scheduled);
  TEST_ASSERT_EQUAL_UINT(1,
                         count_startup_system_events(&fixture, StartupCoordination_start_time_proposal_tag));
  TEST_ASSERT_EQUAL_UINT(0, fixture.event_queue.size);

  size_t system_queue_size = fixture.system_event_queue.size;
  StartupCoordinator_clock_sync_ready(&fixture.startup);
  TEST_ASSERT_EQUAL_UINT(system_queue_size, fixture.system_event_queue.size);
}

void test_failed_initial_clock_step_does_not_release_startup_gate(void) {
  ClockSyncStartupFixture fixture;
  fixture_ctor(&fixture, false);
  complete_startup_handshake(&fixture);
  fake_clock_set_result = LF_ERR;

  deliver_initial_large_offset(&fixture);

  TEST_ASSERT_FALSE(fixture.clock_sync.has_initial_sync);
  TEST_ASSERT_FALSE(fixture.startup.start_time_proposal_scheduled);
  TEST_ASSERT_EQUAL_UINT(0,
                         count_startup_system_events(&fixture, StartupCoordination_start_time_proposal_tag));
}

void test_failed_initial_clock_adjustment_does_not_release_startup_gate(void) {
  ClockSyncStartupFixture fixture;
  fixture_ctor(&fixture, false);
  complete_startup_handshake(&fixture);
  fake_clock_adjust_result = LF_ERR;

  deliver_initial_small_offset(&fixture);

  TEST_ASSERT_FALSE(fixture.clock_sync.has_initial_sync);
  TEST_ASSERT_FALSE(fixture.startup.start_time_proposal_scheduled);
}

Environment* _lf_environment = NULL;

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_clock_sync_constructor_initializes_readiness);
  RUN_TEST(test_non_grandmaster_handshake_does_not_schedule_application_startup_before_sync);
  RUN_TEST(test_grandmaster_preserves_immediate_startup_negotiation);
  RUN_TEST(test_successful_initial_clock_step_releases_startup_gate_once);
  RUN_TEST(test_failed_initial_clock_step_does_not_release_startup_gate);
  RUN_TEST(test_failed_initial_clock_adjustment_does_not_release_startup_gate);
  return UNITY_END();
}
