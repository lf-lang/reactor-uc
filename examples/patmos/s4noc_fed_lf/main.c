#include <stdio.h>
#include <pthread.h>

#include "S4NoCFedLF/r1/src-gen/S4NoCFedLF/r1/lf_start.h"
// #include "S4NoCFedLF/r1/src-gen/S4NoCFedLF/r2/lf_start.h"
// #include "S4NoCFedLF/r2/src-gen/S4NoCFedLF/r1/lf_start.h"
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
    pthread_t thread1, thread2;
    printf("Starting S4NOC Federated LF Example\n");
    
    // Create threads for each federate
    pthread_create(&thread1, NULL, f1_thread, NULL);
    pthread_create(&thread2, NULL, f2_thread, NULL);

    printf("Threads created for federates.\n");

    // Wait for both federates to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("All federates finished.\n");
    return 0; 
}

// int main(void) {
//   printf("Starting S4NOC Federated LF Example\n");
//   return 0;
// }