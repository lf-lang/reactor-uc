#ifndef REACTOR_UC_QUEUES_H
#define REACTOR_UC_QUEUES_H

#include "reactor-uc/error.h"
#include "reactor-uc/event.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/platform.h"

typedef struct EventQueue EventQueue;
typedef struct ReactionQueue ReactionQueue;

/**
 * @brief Min-heap priority event queue ordered by tag.
 */
struct EventQueue {
  /** @brief Return the tag of the earliest event in the queue, or FOREVER_TAG
   * if empty.
   */
  tag_t (*next_tag)(EventQueue* self);
  /**
   * @brief Insert an event into the queue. Returns LF_EVENT_QUEUE_FULL if the
   * queue is full.
   */
  lf_ret_t (*insert)(EventQueue* self, AbstractEvent* event);
  /** @brief Remove and return the earliest event in the queue. Returns
   * LF_EVENT_QUEUE_EMPTY if the queue is empty.
   */
  lf_ret_t (*pop)(EventQueue* self, AbstractEvent* event);
  /** @brief Return true if the queue contains no events. */
  bool (*empty)(EventQueue* self);
  /** @brief Restore the heap invariant for the entire queue. Should be called after bulk
   * insertion of events. */
  void (*build_heap)(EventQueue* self);
  /** @brief Restore the heap invariant downward from @p idx. */
  void (*heapify)(EventQueue* self, size_t idx);
  /**
   * @brief Add @p step to the tag of every queued event, clamping at zero.
   *
   * Used when the physical clock is stepped, so that pending events keep their intended
   * position relative to the corrected clock.
   */
  void (*shift_all_tags)(EventQueue* self, interval_t step);
  /**
   * @brief Find an event with the same tag and trigger as @p event, or NULL if not found.
   *
   * @warning The returned pointer points into the queue's backing array and is only valid
   * for as long as the queue is not mutated. It is NOT safe to hold across a release of
   * the queue mutex: any concurrent `insert` reorders the heap, after which the pointer
   * designates a different event. Callers that want to act on the event they found must
   * use `remove_matching` or `replace_payload`, which do the lookup and the action under
   * a single acquisition of the mutex.
   */
  ArbitraryEvent* (*find_equal_same_tag)(EventQueue* self, AbstractEvent* event);
  /** @brief Remove the event equal to @p event from the queue. Returns
   * LF_EVENT_NOT_FOUND if no such event exists, LF_OK otherwise.
   */
  lf_ret_t (*remove)(EventQueue* self, AbstractEvent* event);
  /**
   * @brief Remove the event matching @p key and hand back its payload.
   *
   * The lookup and the removal happen under a single acquisition of the mutex, so this is
   * safe against concurrent insertion. @p out_payload may be NULL if the caller does not
   * need the payload; otherwise it is written only on success, and the caller takes
   * ownership (the event is no longer reachable through the queue).
   *
   * @return LF_EVENT_NOT_FOUND if no matching event exists, LF_OK otherwise.
   */
  lf_ret_t (*remove_matching)(EventQueue* self, AbstractEvent* key, void** out_payload);
  /**
   * @brief Overwrite the payload of the event matching @p key with @p new_value.
   *
   * The lookup and the copy happen under a single acquisition of the mutex, so this is
   * safe against concurrent insertion.
   *
   * @return LF_EVENT_NOT_FOUND if no matching event exists, LF_INVALID_VALUE if
   * @p new_value is NULL or the matched event has no payload buffer, LF_OK otherwise.
   */
  lf_ret_t (*replace_payload)(EventQueue* self, AbstractEvent* key, const void* new_value, size_t payload_size);

  size_t size;           /**< @brief Current number of events in the queue. */
  size_t capacity;       /**< @brief Maximum number of events the queue can hold. */
  ArbitraryEvent* array; /**< @brief Backing array of the event queue. */
  MUTEX_T mutex;         /**< @brief Mutex protecting concurrent access. */
};

/**
 * @brief Initialize an EventQueue.
 * @param self     The EventQueue to initialize.
 * @param array    Backing array of at least @p capacity ArbitraryEvent elements.
 * @param capacity Maximum number of events the queue can hold.
 */
void EventQueue_ctor(EventQueue* self, ArbitraryEvent* array, size_t capacity);

struct ReactionQueue {
  lf_ret_t (*insert)(ReactionQueue* self, Reaction* reaction);
  Reaction* (*pop)(ReactionQueue* self);
  bool (*empty)(ReactionQueue* self);
  void (*reset)(ReactionQueue* self);

  int* level_size;
  int curr_level;
  int max_active_level;
  int curr_index;
  Reaction** array;
  size_t capacity;
};

void ReactionQueue_ctor(ReactionQueue* self, Reaction** array, int* level_size, size_t capacity);

#endif
