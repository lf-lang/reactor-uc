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
  SystemEvent system_event;
  int neighbor_index;
  ShutdownCoordination msg;
} ShutdownEvent;

/** This struct implements all the necessary logic to gracefully shutdown a federation */
struct ShutdownCoordinator {
  SystemEventHandler super;
  Environment* env;
  size_t longest_path;
  tag_t announcement_of_shutdown;
  tag_t proposed_shutdown_time;
  FederateMessage msg;
  void (*handle_message_callback)(ShutdownCoordinator* self, const ShutdownCoordination* msg, size_t bundle_idx);
  void (*shutdown)(ShutdownCoordinator* self, interval_t shutdown_offset);
};

void ShutdownCoordinator_ctor(ShutdownCoordinator* self, Environment* env, size_t longest_path, size_t payload_size,
                              void* payload_buf, bool* payload_used_buf, size_t payload_buf_capacity);

#endif // REACTOR_UC_SHUTDOWN_COORDINATOR_H
