#ifndef REACTOR_UC_EVENT_H
#define REACTOR_UC_EVENT_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/platform.h"
#include <stdbool.h>

#define EVENT_INIT(Tag, Trigger, Payload)                                                                              \
  {.super.type = EVENT, .super.tag = Tag, .intended_tag = Tag, .trigger = Trigger, .super.payload = Payload}

#define SYSTEM_EVENT_INIT(Tag, Handler, Payload)                                                                       \
  {.super.type = SYSTEM_EVENT, .super.tag = Tag, .super.payload = Payload, .handler = Handler}

typedef struct Trigger Trigger;
typedef struct SystemEventHandler SystemEventHandler;
typedef struct EventPayloadPool EventPayloadPool;

typedef enum { EVENT, SYSTEM_EVENT } EventType;

/** Abstract event type which all other events inherit from. All event pointers can be casted to AbstractEvent. */
typedef struct {
  EventType type;
  tag_t tag;
  void* payload;
} AbstractEvent;

/** Normal reactor event. */
typedef struct {
  AbstractEvent super;
  tag_t intended_tag; // Intended tag used to catch STP violations in federated setting.
  Trigger* trigger;
} Event;

/** System events used to schedule system activities that are unordered wrt reactor events. */
typedef struct {
  AbstractEvent super;
  SystemEventHandler* handler;
} SystemEvent;

/** Arbitrary event is a structure with enough memory to hold any type of event.*/
typedef struct {
  union {
    Event event;
    SystemEvent system_event;
  };
} ArbitraryEvent;

struct EventPayloadPool {
  char* buffer;
  bool* used;
  /** Number of bytes per payload */
  size_t payload_size;
  /**  Max number of allocated payloads*/
  size_t capacity;
  /** Number of payloads reserved to be allocated through `allocate_reserved` */
  size_t reserved;

  MUTEX_T mutex;

  /** Allocate a payload from the pool. */
  lf_ret_t (*allocate)(EventPayloadPool* self, void** payload);
  lf_ret_t (*allocate_reserved)(EventPayloadPool* self, void** payload);
  /** Free a payload. */
  lf_ret_t (*free)(EventPayloadPool* self, void* payload);
};

/** Abstract base class for objects that can handle SystemEvents such as ClockSync and StartupCoordinator. */
struct SystemEventHandler {
  void (*handle)(SystemEventHandler* self, SystemEvent* event);
  EventPayloadPool payload_pool;
};

void EventPayloadPool_ctor(EventPayloadPool* self, char* buffer, bool* used, size_t element_size, size_t capacity,
                           size_t reserved);

/**  
 * @brief Get the tag of an arbitrary event.
 * @param arbitrary_event Pointer to the arbitrary event.
 * @return The tag of the event.
 */
static inline tag_t get_tag(ArbitraryEvent* arbitrary_event) { return arbitrary_event->event.super.tag; }

/**  
 * @brief Compare the tags of two arbitrary events.
 * @param evt1 Pointer to the first arbitrary event.
 * @param evt2 Pointer to the second arbitrary event.
 * @return true if the tags are the same, false otherwise.
 */
static inline bool events_same_tag(ArbitraryEvent* evt1, ArbitraryEvent* evt2) {
  return lf_tag_compare(get_tag(evt1), get_tag(evt2)) == 0;
}

/**  
 * @brief Compare the triggers of two arbitrary events.
 * @param evt1 Pointer to the first arbitrary event.
 * @param evt2 Pointer to the second arbitrary event.
 * @return true if the triggers of both events are the same, false otherwise.
 */
static inline bool events_same_trigger(ArbitraryEvent* evt1, ArbitraryEvent* evt2) {
  if (evt1->event.super.type != EVENT || evt2->event.super.type != EVENT) {
    return false;
  }
  Event* event1 = &evt1->event;
  Event* event2 = &evt2->event;
  return event1->trigger == event2->trigger;
}

/**  
 * @brief Compare the handlers of two arbitrary events.
 * @param evt1 Pointer to the first arbitrary event.
 * @param evt2 Pointer to the second arbitrary event.
 * @return true if the handlers of both events are the same, false otherwise.
 */
static inline bool events_same_handler(ArbitraryEvent* evt1, ArbitraryEvent* evt2) {
  if (evt1->event.super.type != SYSTEM_EVENT || evt2->event.super.type != SYSTEM_EVENT) {
    return false;
  }
  SystemEvent* event1 = &evt1->system_event;
  SystemEvent* event2 = &evt2->system_event;
  return event1->handler == event2->handler;
}

/**  
 * @brief Compare the tags and triggers of two arbitrary events.
 * @param evt1 Pointer to the first arbitrary event.
 * @param evt2 Pointer to the second arbitrary event.
 * @return true if the tags and triggers of both events are the same, false otherwise.
 */
static inline bool events_same_tag_and_trigger(ArbitraryEvent* evt1, ArbitraryEvent* evt2) {
  return events_same_tag(evt1, evt2) && events_same_trigger(evt1, evt2);
}

/**  
 * @brief Compare the tags and handlers of two arbitrary events.
 * @param evt1 Pointer to the first arbitrary event.
 * @param evt2 Pointer to the second arbitrary event.
 * @return true if the tags and handlers of both events are the same, false otherwise.
 */
static inline bool events_same_tag_and_handler(ArbitraryEvent* evt1, ArbitraryEvent* evt2) {
  return events_same_tag(evt1, evt2) && events_same_handler(evt1, evt2);
}

#endif
