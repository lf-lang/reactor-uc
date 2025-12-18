#ifndef S4NOC_FED_COMMON_CONFIG_H
#define S4NOC_FED_COMMON_CONFIG_H

/* ============================================================================
   SHARED CONFIGURATION MACROS - Must be identical between sender and receiver
   ============================================================================ */

// Maximum size of message payload buffer
#define MSG_BUF_SIZE 512

// Number of messages to send/receive before shutdown
#define MAX_MESSAGES 5

// Enable/disable clock synchronization between federates
#define DO_CLOCK_SYNC false

// Startup coordinator: number of neighbors (other federates)
#define NUM_NEIGHBORS 1

// Startup coordinator: number of startup event slots
#define STARTUP_EVENT_SLOTS 6

// Clock sync: number of neighbor clocks to track
#define NUM_NEIGHBOR_CLOCKS 1

// Clock sync: number of clock sync event slots
#define CLOCK_SYNC_EVENT_SLOTS 2

// Number of federated connection bundles
#define NUM_BUNDLES 1

// Number of child reactor instances
#define NUM_CHILD_REACTORS 1

// Serialization/deserialization connection ID (must match between sender and receiver)
#define CONNECTION_ID 0

#endif // S4NOC_FED_COMMON_CONFIG_H
