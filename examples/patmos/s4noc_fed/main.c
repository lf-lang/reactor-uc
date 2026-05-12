
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <machine/patmos.h>
#include <machine/rtc.h>

#include "sender/sender.h"
#include "receiver/receiver.h"

static pthread_mutex_t startup_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t startup_cond = PTHREAD_COND_INITIALIZER;
static int startup_release = 0;

static void wait_for_start_release(void) {
    pthread_mutex_lock(&startup_mutex);
    while (!startup_release) {
        pthread_cond_wait(&startup_cond, &startup_mutex);
    }
    pthread_mutex_unlock(&startup_mutex);
}

static void release_start_threads(void) {
    pthread_mutex_lock(&startup_mutex);
    startup_release = 1;
    pthread_cond_broadcast(&startup_cond);
    pthread_mutex_unlock(&startup_mutex);
}

static void* receiver_thread(void* arg) {
    (void)arg;
    printf("[main] receiver_thread core=%d\n", get_cpuid());
    wait_for_start_release();
    lf_start_receiver();
    return NULL;
}

static void* sender_thread(void* arg) {
    (void)arg;
    printf("[main] sender_thread core=%d\n", get_cpuid());
    wait_for_start_release();
    usleep(50000);
    lf_start_sender();
    return NULL;
}

int main(void) {
    pthread_t receiver_tid;
    pthread_t sender_tid;
    long long cycles_before = get_cpu_cycles();
    printf("Starting S4NOC Federated Example\n");
    fflush(stdout);

    printf("Creating receiver thread...\n");
    fflush(stdout);
    pthread_create(&receiver_tid, NULL, receiver_thread, NULL);
    printf("Receiver thread created\n");
    fflush(stdout);
    
    printf("Creating sender thread...\n");
    fflush(stdout);
    pthread_create(&sender_tid, NULL, sender_thread, NULL);
    printf("Sender thread created\n");
    fflush(stdout);

    printf("Federate threads created, releasing startup gate.\n");
    fflush(stdout);
    release_start_threads();

    printf("Federate threads started.\n");
    fflush(stdout);

    printf("Waiting for receiver thread...\n");
    fflush(stdout);
    pthread_join(receiver_tid, NULL);
    printf("Receiver thread joined\n");
    fflush(stdout);
    
    printf("Waiting for sender thread...\n");
    fflush(stdout);
    pthread_join(sender_tid, NULL);
    printf("Sender thread joined\n");
    fflush(stdout);

    long long cycles_after = get_cpu_cycles();
    printf("Total CPU cycles: %lld\n", cycles_after - cycles_before);
    return 0;
}
