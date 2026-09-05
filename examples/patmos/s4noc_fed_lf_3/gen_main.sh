#!/bin/bash

# Number of federates (default: 3)
N=${N:-3}
rm -rf main.c

cat >> main.c <<EOF
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <machine/rtc.h>
EOF

# Generate include headers for each federate
for i in $(seq 1 $N); do
cat >> main.c <<EOF
#include "S4NoCFedLF3/r$i/src-gen/S4NoCFedLF3/r$i/lf_start.h"
EOF
done

# Generate forward declarations for renamed start functions
cat >> main.c <<EOF

// Forward declarations for the renamed federate start functions
EOF

for i in $(seq 2 $N); do
cat >> main.c <<EOF
void lf_start_$i(void);
EOF
done

cat >> main.c <<EOF

void* f1_thread(void* arg) {
    (void)arg;
    printf("Starting federate 1 on core/thread 1\\n");
    lf_start();
    return NULL;
}
EOF

# Generate thread functions for each federate
for i in $(seq 2 $N); do
cat >> main.c <<EOF

void* f${i}_thread(void* arg) {
    (void)arg;
    printf("Starting federate $i on core/thread $i\\n");
    lf_start_$i();
    return NULL;
}
EOF
done

# Generate main() function
cat >> main.c <<EOF

int main(void) {
    pthread_t threads[$N];
    long long cycles_before = get_cpu_cycles();

    printf("Starting S4NOC Federated LF 3-Reactor Example (Sender -> Repeater -> Receiver)\\n");
EOF

for i in $(seq 1 $N); do
cat >> main.c <<EOF
    int rc_$i = pthread_create(&threads[$((i-1))], NULL, f${i}_thread, NULL);
    if (rc_$i != 0) {
        printf("Failed to create federate $i thread (rc=%d)\\n", rc_$i);
        return 1;
    }
EOF
done

# cat >> main.c <<EOF

#     // Run federate 1 in main thread to avoid hitting thread limits.
#     f1_thread(NULL);

#     printf("Threads created for federates.\\n");

# EOF

for i in $(seq 1 $N); do
cat >> main.c <<EOF
    if (pthread_join(threads[$((i-1))], NULL) != 0) {
        printf("Failed to join federate $i thread\n");
        return 1;
    }
EOF
done

cat >> main.c <<EOF

    printf("All federates finished.\\n");
    long long cycles_after = get_cpu_cycles();
    printf("Total CPU cycles: %lld\\n", cycles_after - cycles_before);
    return 0;
}
EOF
