
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <machine/patmos.h>

#include "sender/sender.h"
#include "repeater/repeater.h"
#include "receiver/receiver.h"

static void* receiver_thread(void* arg) {
    (void)arg;
    printf("[main] receiver_thread core=%d\n", get_cpuid());
    lf_start_receiver();
    return NULL;
}

static void* repeater_thread(void* arg) {
    (void)arg;
    printf("[main] repeater_thread core=%d\n", get_cpuid());
    lf_start_repeater();
    return NULL;
}

static void* sender_thread(void* arg) {
    (void)arg;
    printf("[main] sender_thread core=%d\n", get_cpuid());
    lf_start_sender();
    return NULL;
}

int main(void) {
    pthread_t receiver_tid;
    pthread_t repeater_tid;
    pthread_t sender_tid;

    printf("Starting S4NOC Federated 3-Core Example (Sender -> Repeater -> Receiver), main core=%d\n", get_cpuid());

    pthread_create(&receiver_tid, NULL, receiver_thread, NULL);
    usleep(100000);  // 100ms delay between thread starts
    
    pthread_create(&repeater_tid, NULL, repeater_thread, NULL);
    usleep(100000);  // 100ms delay between thread starts
    
    pthread_create(&sender_tid, NULL, sender_thread, NULL);

    printf("All federate threads started.\n");

    pthread_join(receiver_tid, NULL);
    pthread_join(repeater_tid, NULL);
    pthread_join(sender_tid, NULL);

    printf("All federates finished.\n");
    return 0;
}
