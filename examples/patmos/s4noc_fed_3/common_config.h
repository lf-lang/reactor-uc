#ifndef S4NOC_FED_3_COMMON_CONFIG_H
#define S4NOC_FED_3_COMMON_CONFIG_H

/* ============================================================================
   SHARED CONFIGURATION MACROS - Must be identical between all 3 federates
   (sender, repeater, receiver)
   ============================================================================ */

// Maximum size of message payload buffer
#define MSG_BUF_SIZE 512

// Number of messages to send/receive before shutdown
#define MAX_MESSAGES 1

// Enable/disable clock synchronization between federates
#define DO_CLOCK_SYNC false

// Startup coordinator: number of startup event slots
#define STARTUP_EVENT_SLOTS 6

// Clock sync: number of clock sync event slots
#define CLOCK_SYNC_EVENT_SLOTS 2

// Number of child reactor instances
#define NUM_CHILD_REACTORS 1

// Serialization/deserialization connection IDs
#define CONNECTION_ID_IN 0
#define CONNECTION_ID_OUT 0

#endif // S4NOC_FED_3_COMMON_CONFIG_H
