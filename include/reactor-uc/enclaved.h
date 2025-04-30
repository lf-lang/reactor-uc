#ifndef REACTOR_UC_ENCLAVED_H
#define REACTOR_UC_ENCLAVED_H

#include "reactor-uc/platform.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/connection.h"

typedef struct EnclavedConnection EnclavedConnection;

struct EnclavedConnection {
  Connection super;
  interval_t delay;
  ConnectionType type;
  EventPayloadPool payload_pool;
  void *staged_payload_ptr;
  MUTEX_T mutex; // Used to protect the _last_known_tag variable.
  tag_t _last_known_tag;
  void (*set_last_known_tag)(EnclavedConnection *self, tag_t tag);
  tag_t (*get_last_known_tag)(EnclavedConnection *self);
};

void EnclavedConnection_ctor(EnclavedConnection *self, Reactor *parent, Port **downstreams, size_t num_downstreams,
                             interval_t delay, ConnectionType type, size_t payload_size, void *payload_buf,
                             bool *payload_used_buf, size_t payload_buf_capacity);

#endif