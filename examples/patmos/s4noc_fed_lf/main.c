#include <stdio.h>
#include <pthread.h>

#include "S4NoCFedLF/r1/src-gen/S4NoCFedLF/r1/lf_start.h"
#include "S4NoCFedLF/r2/src-gen/S4NoCFedLF/r2/lf_start.h"

void* f1_thread(void* arg) {
    printf("Starting federate 1 on core/thread 1\n");
    lf_start();
    return NULL;
}

void* f2_thread(void* arg) {
    printf("Starting federate 2 on core/thread 2\n");
    lf_start_2();
    return NULL;
}

int main(void) {
    pthread_t threads[2];
    printf("Starting S4NOC Federated LF Example\n");
    pthread_create(&threads[0], NULL, f1_thread, NULL);
    pthread_create(&threads[1], NULL, f2_thread, NULL);

    printf("Threads created for federates.\n");

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    printf("All federates finished.\n");
    return 0;
}
