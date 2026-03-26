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
  ShutdownEvent system_event;
  Environment* env;
  size_t longest_path;
  tag_t announcement_of_shutdown;
  interval_t proposed_shutdown_time;
  FederateMessage msg;
  void (*handle_message_callback)(ShutdownCoordinator* self, const ShutdownCoordination* msg, size_t bundle_idx);
  void (*shutdown)(ShutdownCoordinator* self, interval_t shutdown_offset);
};

void ShutdownCoordinator_ctor(ShutdownCoordinator* self, Environment* env, size_t longest_path);

#endif // REACTOR_UC_SHUTDOWN_COORDINATOR_H
