#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/environments/federated_environment.h"
#include "unity.h"
#include "test_util.h"
#include <string.h>
#include <pthread.h>

#define MESSAGE_CONTENT "Hello S4NOC"
#define MESSAGE_CONNECTION_ID 42
#define SOURCE_CORE 1
#define DESTINATION_CORE 2

Reactor parent;
FederatedEnvironment fed_env;
Environment *_lf_environment = &fed_env.super;
FederatedConnectionBundle sender_bundle;
FederatedConnectionBundle receiver_bundle;
FederatedConnectionBundle *net_bundles[] = {&sender_bundle, &receiver_bundle};
StartupCoordinator startup_coordinator;

S4NOCPollChannel sender_channel;
S4NOCPollChannel receiver_channel;
NetworkChannel *sender = (NetworkChannel *)&sender_channel.super;
NetworkChannel *receiver = (NetworkChannel *)&receiver_channel.super;

bool receiver_callback_called = false;

void setUp(void) {
    FederatedEnvironment_ctor(&fed_env, &parent, NULL, false, net_bundles, 2, &startup_coordinator, NULL);

    S4NOCPollChannel_ctor(&sender_channel, &fed_env.super, DESTINATION_CORE);
    S4NOCPollChannel_ctor(&receiver_channel, &fed_env.super, SOURCE_CORE);

    FederatedConnectionBundle_ctor(&sender_bundle, &parent, sender, NULL, NULL, 0, NULL, NULL, 0, 0);
    FederatedConnectionBundle_ctor(&receiver_bundle, &parent, receiver, NULL, NULL, 0, NULL, NULL, 0, 0);

    s4noc_global_state.core_channels[SOURCE_CORE][DESTINATION_CORE] = &receiver_channel;
}

void tearDown(void) {
    sender->free(sender);
    receiver->free(receiver);
}

void test_open_connection(void) {
    TEST_ASSERT_OK(sender->open_connection(sender));
    TEST_ASSERT_OK(receiver->open_connection(receiver));

    TEST_ASSERT_TRUE(sender->is_connected(sender));
    TEST_ASSERT_TRUE(receiver->is_connected(receiver));
}

void receiver_callback_handler(FederatedConnectionBundle *self, const FederateMessage *_msg) {
    LF_INFO(NET, "Receiver callback handler called");
    (void)self;
    const TaggedMessage *msg = &_msg->message.tagged_message;
    printf("\nReceiver: Received message with connection number %i and content %s\n", msg->conn_id,
           (char *)msg->payload.bytes);
    TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char *)msg->payload.bytes);
    TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

    receiver_callback_called = true;
}

void test_sender_send_and_receiver_recv(void) {
    TEST_ASSERT_OK(sender->open_connection(sender));
    TEST_ASSERT_OK(receiver->open_connection(receiver));

    receiver->register_receive_callback(receiver, receiver_callback_handler, NULL);

    FederateMessage msg;
    msg.which_message = FederateMessage_tagged_message_tag;

    TaggedMessage *port_message = &msg.message.tagged_message;
    port_message->conn_id = MESSAGE_CONNECTION_ID;
    const char *message = MESSAGE_CONTENT;
    memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
    port_message->payload.size = sizeof(MESSAGE_CONTENT);
    LF_INFO(NET, "Sender: Sending message with connection number %i and content %s\n", port_message->conn_id,
           (char *)port_message->payload.bytes);
    TEST_ASSERT_OK(sender->send_blocking(sender, &msg));
    LF_INFO(NET, "Sender: Message sent\n");
    ((PolledNetworkChannel *)&receiver_channel.super)->poll((PolledNetworkChannel *)&receiver_channel.super);
    LF_INFO(NET, "Receiver: Message polled\n");
    TEST_ASSERT_TRUE(receiver_callback_called);
}

int main(void) {
    UNITY_BEGIN();
    //RUN_TEST(test_open_connection);
    RUN_TEST(test_sender_send_and_receiver_recv);
    return UNITY_END();
}