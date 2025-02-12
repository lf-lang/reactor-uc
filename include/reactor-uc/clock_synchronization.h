#ifndef REACTOR_UC_CLOCK_SYNCHRONIZATION_H
#define REACTOR_UC_CLOCK_SYNCHRONIZATION_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "proto/message.pb.h"

typedef struct ClockSynchronization ClockSynchronization;
typedef struct Environment Environment;

typedef struct {
  int priority;
} NeighborClock;

struct ClockSynchronization {
  Environment *env;
  NeighborClock *neighbor_clock;
  size_t num_neighbours;
  bool is_grandmaster;
  bool has_initial_sync;
  int master_neighbor_index;
  FederateMessage msg;
  void (*handle_message_callback)(ClockSynchronization *self, const ClockSyncMessage *msg, size_t bundle_idx);
  lf_ret_t (*ask_neighbors_for_priorities)(ClockSynchronization *self);
  lf_ret_t (*update_priorities)(ClockSynchronization *self);
};

void ClockSynchronization_ctor(ClockSynchronization *self, Environment *env, NeighborClock *neighbor_clock,
                             size_t num_neighbors, bool is_grandmaster);


#endif // REACTOR_UC_CLOCK_SYNCHRONIZATION_H