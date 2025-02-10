#ifndef REACTOR_UC_STARTUP_COORDINATOR_H
#define REACTOR_UC_STARTUP_COORDINATOR_H

#include "reactor-uc/error.h"
#include "proto/message.pb.h"

typedef StartupCoordinator StartupCoordinator;
typedef Environment Environment;

typedef enum StartupCoordinatorState {
  STARTUP_COORDINATOR_STATE_UNINITIALIZED,
  STARTUP_COORDINATOR_STATE_HANDSHAKE,
  STARTUP_COORDINATOR_STATE_NEGOTIATE_START_TAG,
  STARTUP_COORDINATOR_STATE_DONE
} StartupCoordinatorState;

typedef struct {
  bool handshake_requested;
  bool handshake_received;
  int start_time_received_counter;
} NeighborState;


struct StartupCoordinator {
  // FIXME: INitialize all of these to proper values in CTOR.
 A Environment *env;
  size_t longest_path;
  StartupCoordinatorState state;
  NeighborState neighbor_state;
  FederateMessage msg;
  instant_t start_time_proposal;
  int start_time_proposal_step;
  size_t num_neighbours;
  void (*handle_message_callback)(StartupCoordinator *self, const FederateMessage *msg);
  lf_ret_t (*perform_handshake)(StartupCoordinator *self);
  lf_ret_t (*negotiate_start_tag)(StartupCoordinator *self);
};

void StartupCoordinator_ctor(StartupCoordinator *self, Environment *env, NeighborState *neighbor_state, size_t num_neighbors, size_t longest_path);

#endif // REACTOR_UC_STARTUP_COORDINATOR_H