#ifndef REACTOR_UC_EVENT_H
#define REACTOR_UC_EVENT_H

#include "reactor-uc/tag.h"
#include "reactor-uc/trigger.h"

typedef struct {
  tag_t tag;
  Trigger *trigger;
  void *payload;
} Event;

typedef struct {
  char *buffer;
  bool *used;
  size_t size;
  size_t capacity;
  lf_ret_t (*allocate)(EventPayloadPool *self, void **payload);
  lf_ret_t (*free)(EventPayloadPool *self, void *payload);
} EventPayloadPool;

void EventPayloadPool_ctor(EventPayloadPool *self, char *buffer, bool *used, size_t size, size_t capacity);

#endif
