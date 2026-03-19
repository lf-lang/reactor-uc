#ifndef REACTOR_UC_SHUTDOWN_COORDINATOR_H
#define REACTOR_UC_SHUTDOWN_COORDINATOR_H

#include "reactor-uc/environment.h"
#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/event.h"
#include "proto/message.pb.h"

typedef struct ShutdownCoordinator ShutdownCoordinator;

/** The payload of a StartupCoordinator event. */
typedef struct {
  int neighbor_index;
  ShutdownCoordination msg;
} ShutdownEvent;


/** This struct implements all the necessary logic to gracefully shutdown a federation */
struct ShutdownCoordinator {
  SystemEventHandler super;
  Environment* env;
  size_t num_neighbours;
  size_t longest_path;
  uint32_t proposed_shutdown_time;
  FederateMessage msg;
  void (*handle_message_callback)(ShutdownCoordinator* self, const ShutdownCoordination* msg, size_t bundle_idx);
};

#endif // REACTOR_UC_SHUTDOWN_COORDINATOR_H
