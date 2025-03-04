#ifndef REACTOR_UC_COORDINATOR_H
#define REACTOR_UC_COORDINATOR_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/environment.h"

typedef struct Coordinator Coordinator;


struct Coordinator {
  SystemEventHandler super;
  Environment *env;



  void (*logical_tag_complete)(Coordinator *self, tag_t next_local_event_tag);
  void (*handle_message_callback)(Coordinator *self, const void *msg, size_t bundle_idx);
};

#endif // REACTOR_UC_COORDINATOR_H