#include <stdio.h>
#include <pthread.h>
#include "sender/sender.h"
#include "receiver/receiver.h"

static void* receiver_thread(void* arg) {
    (void)arg;
    lf_start_receiver();
    return NULL;
}

static void* sender_thread(void* arg) {
    (void)arg;
    lf_start_sender();
    return NULL;
}

int main(void) {
    pthread_t threads[2]; 
    printf("Starting S4NOC Federated Example\n");
    pthread_create(&threads[0], NULL, receiver_thread, NULL);
    pthread_create(&threads[1], NULL, sender_thread, NULL);

    printf("Threads created for federates.\n");

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    printf("All federates finished.\n");
    return 0;
}
