#ifndef REACTOR_UC_EVENT_H
#define REACTOR_UC_EVENT_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include <stdbool.h>

#define EVENT_INIT(Tag, Trigger, Payload) {.tag = Tag, .intended_tag = Tag, .trigger = Trigger, .payload = Payload}

typedef struct Trigger Trigger;

typedef struct {
  tag_t tag;          // The tag the event should be handled at
  tag_t intended_tag; // The intended tag of the event. Can be different from tag for federated input events.
  Trigger *trigger;   // A pointer to the trigger that caused the event.
  void *payload;      // A pointer to the payload of the event.
} Event;

typedef struct EventPayloadPool EventPayloadPool;

struct EventPayloadPool {
  char *buffer;
  bool *used;
  size_t size;
  size_t capacity;
  lf_ret_t (*allocate)(EventPayloadPool *self, void **payload);
  lf_ret_t (*free)(EventPayloadPool *self, void *payload);
};

void EventPayloadPool_ctor(EventPayloadPool *self, char *buffer, bool *used, size_t size, size_t capacity);

#endif
