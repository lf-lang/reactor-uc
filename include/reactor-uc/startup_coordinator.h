#ifndef REACTOR_UC_STARTUP_COORDINATOR_H
#define REACTOR_UC_STARTUP_COORDINATOR_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/event.h"
#include "reactor-uc/platform.h"
#include "proto/message.pb.h"

typedef struct StartupCoordinator StartupCoordinator;
typedef struct Environment Environment;

/** Represents the state of a neighbor. */
typedef struct {
  bool handshake_response_received;     // Whether a handshake response has been received from this neighbor.
  bool handshake_request_received;      // Whether a handshake response has been sent to this neighbor.
  bool handshake_response_sent;         // Whether a handshake response has been sent to this neighbor.
  size_t start_time_proposals_received; // The number of start time proposals received from this neighbor.
} NeighborState;

/** The payload of a StartupCoordinator event. */
typedef struct {
  int neighbor_index;
  StartupCoordination msg;
} StartupEvent;

/** This structure is used to coordinate the startup of the federation. */
struct StartupCoordinator {
  SystemEventHandler super;
  Environment *env;
  size_t longest_path;
  StartupCoordinationState state;
  NeighborState *neighbor_state;
  size_t num_neighbours;
  uint32_t start_time_proposal_step;
  FederateMessage msg;
  instant_t start_time_proposal;
  void (*handle_message_callback)(StartupCoordinator *self, const StartupCoordination *msg, size_t bundle_idx);
  lf_ret_t (*connect_to_neighbors_blocking)(StartupCoordinator *self);
  void (*start)(StartupCoordinator *self);
};

void StartupCoordinator_ctor(StartupCoordinator *self, Environment *env, NeighborState *neighbor_state,
                             size_t num_neighbors, size_t longest_path, size_t payload_size, void *payload_buf,
                             bool *payload_used_buf, size_t payload_buf_capacity);

#endif // REACTOR_UC_STARTUP_COORDINATOR_H