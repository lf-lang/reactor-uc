#!/bin/bash

# Number of federates
N=2
rm -rf main.c

cat >> main.c <<EOF
#include <stdio.h>
#include <pthread.h>

EOF

# Generate include headers for each federate
for i in $(seq 1 $N); do
cat >> main.c <<EOF
#include "S4NoCFedLF/r$i/src-gen/S4NoCFedLF/r$i/lf_start.h"
EOF
done

cat >> main.c <<EOF

void* f1_thread(void* arg) {
    printf("Starting federate 1 on core/thread 1\\n");
    lf_start();
    return NULL;
}
EOF

# Generate thread functions for each federate
for i in $(seq 2 $N); do
cat >> main.c <<EOF

void* f${i}_thread(void* arg) {
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
    printf("Starting S4NOC Federated LF Example\\n");
EOF

for i in $(seq 1 $N); do
cat >> main.c <<EOF
    pthread_create(&threads[$((i-1))], NULL, f${i}_thread, NULL);
EOF
done

cat >> main.c <<EOF

    printf("Threads created for federates.\\n");

EOF

for i in $(seq 1 $N); do
cat >> main.c <<EOF
    pthread_join(threads[$((i-1))], NULL);
EOF
done

cat >> main.c <<EOF

    printf("All federates finished.\\n");
    return 0;
}
EOF
