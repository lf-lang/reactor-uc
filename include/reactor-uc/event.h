#ifndef REACTOR_UC_EVENT_H
#define REACTOR_UC_EVENT_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include <stdbool.h>

#define EVENT_INIT(Tag, Trigger, Payload)                                                                              \
  {.super.type = EVENT, .super.tag = Tag, .intended_tag = Tag, .trigger = Trigger, .super.payload = Payload}

#define SYSTEM_EVENT_INIT(Tag, Handler, Payload)                                                                       \
  {.super.type = SYSTEM_EVENT, .super.tag = Tag, .super.payload = Payload, .handler = Handler}

typedef struct Trigger Trigger;
typedef struct SystemEventHandler SystemEventHandler;
typedef struct EventPayloadPool EventPayloadPool;

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

struct EventPayloadPool {
  char *buffer;
  bool *used;
  /** Number of bytes per payload */
  size_t payload_size;
  /** Number of allocated payloads */
  size_t num_allocated;
  /**  Max number of allocated payloads*/
  size_t capacity;

  /** Allocate a payload from the pool. */
  lf_ret_t (*allocate)(EventPayloadPool *self, void **payload);
  /** Allocate a payload but only if we have more available than num_reserved. */
  lf_ret_t (*allocate_with_reserved)(EventPayloadPool *self, void **payload, size_t num_reserved);
  /** Free a payload. */
  lf_ret_t (*free)(EventPayloadPool *self, void *payload);
};

/** Abstract base class for objects that can handle SystemEvents such as ClockSync and StartupCoordinator. */
struct SystemEventHandler {
  void (*handle)(SystemEventHandler *self, SystemEvent *event);
  EventPayloadPool payload_pool;
};

void EventPayloadPool_ctor(EventPayloadPool *self, char *buffer, bool *used, size_t element_size, size_t capacity);

#endif
