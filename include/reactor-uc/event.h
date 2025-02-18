#ifndef REACTOR_UC_EVENT_H
#define REACTOR_UC_EVENT_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include <stdbool.h>

#define EVENT_INIT(Tag , Trigger, Payload)                                                                              \
  {.super.type = EVENT, .super.tag = Tag, .intended_tag = Tag, .trigger = Trigger, .super.payload = Payload}

#define SYSTEM_EVENT_INIT(Tag, Handler, Payload)                                                                                \
  {.super.type = SYSTEM_EVENT, .super.tag = Tag, .super.payload = Payload, .handler = Handler}

typedef struct Trigger Trigger;
typedef struct SystemEventHandler SystemEventHandler;

typedef enum { EVENT, SYSTEM_EVENT } EventType;

/** Abstract event type which all other events inherit from. */
typedef struct {
  EventType type;
  tag_t tag;
  void *payload;
} AbstractEvent;

/** Normal reactor event. */
typedef struct {
  AbstractEvent super;
  tag_t intended_tag; // Intended tag used to catch STP violations in federated setting.
  Trigger *trigger;
} Event;

/** System events used to schedule system activities that are unordered wrt reactor events. */
typedef struct {
  AbstractEvent super;
  SystemEventHandler *handler;
} SystemEvent;

/** Arbitrary event can hold any event.*/
typedef struct {
  union {
    Event event;
    SystemEvent system_event;
  };
} ArbitraryEvent;

struct SystemEventHandler {
  void (*handle)(SystemEventHandler *self, SystemEvent *event);
  EventPayloadPool payload_pool;
};

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
