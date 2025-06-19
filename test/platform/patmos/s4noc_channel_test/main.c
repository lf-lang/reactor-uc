#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/environments/federated_environment.h"
#include "unity.h"
#include "test_util.h"
#include <string.h>

#define MESSAGE_CONTENT "Hello S4NOC"
#define MESSAGE_CONNECTION_ID 42
#define SOURCE_CORE 1
#define DESTINATION_CORE 2
#define MAX_TRIES 10

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
    /* init environment */
    FederatedEnvironment_ctor(&fed_env, &parent, NULL, false, net_bundles, 2, &startup_coordinator, NULL);

    /* init channel */
    S4NOCPollChannel_ctor(&sender_channel, DESTINATION_CORE);
    S4NOCPollChannel_ctor(&receiver_channel, SOURCE_CORE);

    /* init bundles */
    FederatedConnectionBundle_ctor(&sender_bundle, &parent, sender, NULL, NULL, 0, NULL, NULL, 0, 0);
    FederatedConnectionBundle_ctor(&receiver_bundle, &parent, receiver, NULL, NULL, 0, NULL, NULL, 0, 0);

    s4noc_global_state.core_channels[SOURCE_CORE][DESTINATION_CORE] = &receiver_channel;
    s4noc_global_state.core_channels[DESTINATION_CORE][SOURCE_CORE] = &sender_channel;

}

void tearDown(void) {
    sender->free(sender);
    receiver->free(receiver);
}

void receiver_callback_handler(FederatedConnectionBundle *self, const FederateMessage *_msg) {
    (void)self;
    const TaggedMessage *msg = &_msg->message.tagged_message;
    LF_INFO(NET,"Receiver: Received message with connection number %i and content %s\n", msg->conn_id,
           (char *)msg->payload.bytes);
    TEST_ASSERT_EQUAL_STRING(MESSAGE_CONTENT, (char *)msg->payload.bytes);
    TEST_ASSERT_EQUAL(MESSAGE_CONNECTION_ID, msg->conn_id);

    receiver_callback_called = true;
}

void send_message(void) {
    FederateMessage msg;
    msg.which_message = FederateMessage_tagged_message_tag;

    TaggedMessage *port_message = &msg.message.tagged_message;
    port_message->conn_id = MESSAGE_CONNECTION_ID;
    const char *message = MESSAGE_CONTENT;
    memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
    port_message->payload.size = sizeof(MESSAGE_CONTENT);
    LF_INFO(NET, "Sender: Sending message with connection number %i and content %s\n", port_message->conn_id, (char *)port_message->payload.bytes);
    TEST_ASSERT_OK(sender->send_blocking(sender, &msg));
}

void receive_message(void) {
    int tries = 0;
    do {
        LF_DEBUG(NET, "Receiver: Polling for messages, tries: %i\n", tries);
        tries++;
        ((PolledNetworkChannel *)&receiver_channel.super)->poll((PolledNetworkChannel *)&receiver_channel.super);
    } while (receiver_callback_called == false && tries < MAX_TRIES);
}

void test_sender_send_and_receiver_recv(void) {
    TEST_ASSERT_OK(sender->open_connection(sender));
    TEST_ASSERT_OK(receiver->open_connection(receiver));

    receiver->register_receive_callback(receiver, receiver_callback_handler, NULL);

    if(pthread_create(&sender_channel.worker_thread, NULL, send_message, NULL)) {
        LF_ERR(NET,"Error creating thread\n");
        return;
    }
    if(pthread_create(&receiver_channel.worker_thread, NULL, receive_message, NULL)) {
        LF_ERR(NET,"Error creating thread\n");
        return;
    }
    if (pthread_join(sender_channel.worker_thread, NULL)) {
        LF_ERR(NET,"Error joining thread\n");
        return;
    }
    if (pthread_join(receiver_channel.worker_thread, NULL)) {
        LF_ERR(NET,"Error joining thread\n");
        return;
    }

    TEST_ASSERT_TRUE(receiver_callback_called);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sender_send_and_receiver_recv);
    return UNITY_END();
}
