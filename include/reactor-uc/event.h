#ifndef REACTOR_UC_EVENT_H
#define REACTOR_UC_EVENT_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include <stdbool.h>

#define EVENT_INIT(Tag, Trigger, Payload)                                                                              \
  {.super.type = EVENT, .super.tag = Tag, .intended_tag = Tag, .trigger = Trigger, .super.payload = Payload}

#define SYSTEM_EVENT_INIT(Tag, Payload)                                                                                \
  {.super.type = EVENT, .super.tag = Tag, .trigger = Trigger, .super.payload = Payload}

#define SYSTEM_EVENT_MICROSTEP -1

typedef struct Trigger Trigger;
typedef struct SystemEventHandler SystemEventHandler;

typedef enum { EVENT, SYSTEM_EVENT } EventType;

typedef struct {
  EventType type;
  tag_t tag;
  void *payload;
} AbstractEvent;

typedef struct {
  AbstractEvent super;
  tag_t intended_tag;
  Trigger *trigger;
} Event;

typedef struct {
  AbstractEvent super;
  SystemEventHandler *handler;
} SystemEvent;

typedef struct {
  union {
    Event event;
    SystemEvent system_event;
  };
} ArbitraryEvent;

struct SystemEventHandler {
  void (*handle)(SystemEventHandler *self, SystemEvent *event);
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
