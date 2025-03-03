#ifndef REACTOR_UC_CLOCK_SYNCHRONIZATION_H
#define REACTOR_UC_CLOCK_SYNCHRONIZATION_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/event.h"
#include "proto/message.pb.h"

#define CLOCK_SYNC_DEFAULT_PERIOD SEC(1)
#define CLOCK_SYNC_DEFAULT_KP 0.7            // Default value from linuxptp
#define CLOCK_SYNC_DEFAULT_KI 0.3            // Default value from linuxptp
#define CLOCK_SYNC_DEFAULT_MAX_ADJ 200000000 // This is the default max-ppb value for linuxptp

typedef struct ClockSynchronization ClockSynchronization;
typedef struct Environment Environment;

typedef struct {
  int priority;
} NeighborClock;

typedef struct {
  int neighbor_index;
  ClockSyncMessage msg;
} ClockSyncEvent;

typedef struct {
  float Kp;
  float Ki;
  interval_t accumulated_error;
  interval_t last_error;
  interval_t clamp;
} ClockServo;

typedef struct {
  instant_t t1;
  instant_t t2;
  instant_t t3;
  instant_t t4;
} ClockSyncTimestamps;

struct ClockSynchronization {
  SystemEventHandler super;
  Environment *env;
  NeighborClock *neighbor_clock;
  size_t num_neighbours;
  bool is_grandmaster;
  int master_neighbor_index;
  int sequence_number;
  interval_t period;
  ClockSyncTimestamps timestamps;
  ClockServo servo;
  FederateMessage msg;
  ClockSyncEvent
      clock_sync_timer_event; // This is the payload for the periodic event that starts of a clock sync round.
  void (*handle_message_callback)(ClockSynchronization *self, const ClockSyncMessage *msg, size_t bundle_idx);
};

void ClockSynchronization_ctor(ClockSynchronization *self, Environment *env, NeighborClock *neighbor_clock,
                               size_t num_neighbors, bool is_grandmaster, size_t payload_size, void *payload_buf,
                               bool *payload_used_buf, size_t payload_buf_capacity, interval_t period,
                               interval_t max_adj, float servo_kp, float servo_ki);

#endif // REACTOR_UC_CLOCK_SYNCHRONIZATION_H