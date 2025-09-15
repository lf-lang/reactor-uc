#ifndef REACTOR_UC_STARTUP_COORDINATOR_H
#define REACTOR_UC_STARTUP_COORDINATOR_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/event.h"
#include "reactor-uc/reactor.h"
#include "proto/message.pb.h"

typedef struct StartupCoordinator StartupCoordinator;
typedef struct Environment Environment;
typedef enum JoiningPolicy JoiningPolicy;
typedef struct TimerConfig TimerConfig;

enum JoiningPolicy { JOIN_IMMEDIATELY = 0, JOIN_TIMER_ALIGNED = 1, JOIN_AT_HYPER_PERIOD = 2 };

/** Represents the state of a neighbor. */
typedef struct {
  /**True, if this federate needs to be present during joining*/
  bool core_federate;
  /** True, if a handshake response has been received from this neighbor.*/
  bool handshake_response_received;
  /** True, if a handshake response has been sent to this neighbor.*/
  bool handshake_request_received;
  /** True, if a handshake response has been sent to this neighbor.*/
  bool handshake_response_sent;
  /** The number of start time proposals received from this neighbor.*/
  size_t start_time_proposals_received;
  /** Saves the initial state of the neighbor.*/
  StartupCoordinationState initial_state_of_neighbor;
  /** Used by transient federates, to figure out the current logical times of all neighboring federates. */
  interval_t current_logical_time;
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
  JoiningPolicy joining_policy;
  void (*handle_message_callback)(StartupCoordinator *self, const StartupCoordination *msg, size_t bundle_idx);
  lf_ret_t (*connect_to_neighbors_blocking)(StartupCoordinator *self);
  void (*start)(StartupCoordinator *self);
};

void StartupCoordinator_ctor(StartupCoordinator *self, Environment *env, NeighborState *neighbor_state,
                             size_t num_neighbors, size_t longest_path, JoiningPolicy joining_policy,
                             size_t payload_size, void *payload_buf, bool *payload_used_buf,
                             size_t payload_buf_capacity);

#endif // REACTOR_UC_STARTUP_COORDINATOR_H
