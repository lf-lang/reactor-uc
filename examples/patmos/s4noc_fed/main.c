
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <machine/patmos.h>
#include <machine/rtc.h>

#include "sender/sender.h"
#include "receiver/receiver.h"

static pthread_mutex_t startup_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t startup_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t uart_lock = PTHREAD_MUTEX_INITIALIZER;  // Mutex for coordinating UART output across cores
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
    pthread_mutex_lock(&uart_lock);
    printf("[main] receiver_thread core=%d\n", get_cpuid());
    pthread_mutex_unlock(&uart_lock);
    wait_for_start_release();
    lf_start_receiver();
    return NULL;
}

static void* sender_thread(void* arg) {
    (void)arg;
    pthread_mutex_lock(&uart_lock);
    printf("[main] sender_thread core=%d\n", get_cpuid());
    pthread_mutex_unlock(&uart_lock);
    wait_for_start_release();
    usleep(50000);
    lf_start_sender();
    return NULL;
}

int main(void) {
    pthread_t receiver_tid;
    pthread_t sender_tid;
    long long cycles_before = get_cpu_cycles();
    
    pthread_mutex_lock(&uart_lock);
    printf("Starting S4NOC Federated Example\n");
    fflush(stdout);
    pthread_mutex_unlock(&uart_lock);

    pthread_create(&receiver_tid, NULL, receiver_thread, NULL);

    pthread_mutex_lock(&uart_lock);
    printf("Creating sender thread...\n");
    fflush(stdout);
    pthread_mutex_unlock(&uart_lock);

    pthread_create(&sender_tid, NULL, sender_thread, NULL);

    pthread_mutex_lock(&uart_lock);
    printf("Federate threads created, releasing startup gate.\n");
    fflush(stdout);
    pthread_mutex_unlock(&uart_lock);

    release_start_threads();
    
    pthread_mutex_lock(&uart_lock);
    printf("Federate threads started.\n");
    fflush(stdout);
    pthread_mutex_unlock(&uart_lock);

    pthread_join(receiver_tid, NULL);

    pthread_mutex_lock(&uart_lock);
    printf("Receiver thread joined\n");
    fflush(stdout);
    pthread_mutex_unlock(&uart_lock);

    pthread_join(sender_tid, NULL);

    pthread_mutex_lock(&uart_lock);
    printf("Sender thread joined\n");
    fflush(stdout);
    pthread_mutex_unlock(&uart_lock);

    long long cycles_after = get_cpu_cycles();
    printf("Total CPU cycles: %lld\n", cycles_after - cycles_before);
    return 0;
}
