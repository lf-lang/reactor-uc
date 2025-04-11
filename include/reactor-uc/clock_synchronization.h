#ifndef REACTOR_UC_CLOCK_SYNCHRONIZATION_H
#define REACTOR_UC_CLOCK_SYNCHRONIZATION_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/event.h"
#include "proto/message.pb.h"
#include "reactor-uc/platform.h"

#define CLOCK_SYNC_DEFAULT_PERIOD SEC(1)
#define CLOCK_SYNC_DEFAULT_KP 0.7            // Default value from linuxptp
#define CLOCK_SYNC_DEFAULT_KI 0.3            // Default value from linuxptp
#define CLOCK_SYNC_DEFAULT_MAX_ADJ 200000000 // This is the default max-ppb value for linuxptp
#define CLOCK_SYNC_INITAL_STEP_THRESHOLD MSEC(100)

typedef struct ClockSynchronization ClockSynchronization;
typedef struct Environment Environment;

typedef struct {
  int priority; // The clock-sync priority of a neighbor, lower is better.
} NeighborClock;

/** The payload of a clock-sync system event */
typedef struct {
  int neighbor_index;   // The index of the neighbor that the event is for.
  ClockSyncMessage msg; // The message received from that neighbor.
} ClockSyncEvent;

/** Parameters and state needed by the clock servo. */
typedef struct {
  float Kp;                     // Kp of the PID controller
  float Ki;                     // Ki of the PID controller.
  interval_t accumulated_error; // The accumulated error (multiplied by Ki)
  interval_t last_error;        // The error from the last iteration (used to calculate the derivative)
  interval_t clamp;             // The maximum adjustment that the servo can make in one iteration.
} ClockServo;

/** The four timestamps used to calculate the one-way-delay + clock offset between a slave and a master. */
typedef struct {
  instant_t t1; // Time when the master sent the sync message.
  instant_t t2; // Time when the slave received the sync message.
  instant_t t3; // Time when the slave sent the follow-up message.
  instant_t t4; // Time when the master received the follow-up message.
} ClockSyncTimestamps;

struct ClockSynchronization {
  SystemEventHandler super; // ClockSynchronization is a subclass of SystemEventHandler
  Environment *env;
  NeighborClock *neighbor_clock;  // Pointer to an array of neighbor clocks, one for each neighbor.
  size_t num_neighbours;          // Number of neighbors, length of the neighbor_clock array.
  bool is_grandmaster;            // Whether this node is the grandmaster.
  bool has_initial_sync;          // Whether the initial sync has been completed.
  int master_neighbor_index;      // The index of the master neighbor, if this node is not the grandmaster.
  int my_priority;                // The priority of this node.
  int sequence_number;            // The sequence number of the last sent sync request message (if slave).
  interval_t period;              // The period between sync request messages are sent to the neighbor master.
  ClockSyncTimestamps timestamps; // The timestamps used to compute clock offset.
  ClockServo servo;               // The PID controller
  void (*handle_message_callback)(ClockSynchronization *self, const ClockSyncMessage *msg, size_t bundle_idx);
};

void ClockSynchronization_ctor(ClockSynchronization *self, Environment *env, NeighborClock *neighbor_clock,
                               size_t num_neighbors, bool is_grandmaster, size_t payload_size, void *payload_buf,
                               bool *payload_used_buf, size_t payload_buf_capacity, interval_t period,
                               interval_t max_adj, float servo_kp, float servo_ki);

#endif // REACTOR_UC_CLOCK_SYNCHRONIZATION_H
