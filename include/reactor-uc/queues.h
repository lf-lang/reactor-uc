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
  /** @brief Find an event with the same tag and trigger as @p event, or NULL if not found. */
  ArbitraryEvent* (*find_equal_same_tag)(EventQueue* self, AbstractEvent* event);
  /** @brief Remove the event equal to @p event from the queue. If the event is not found, it is considered a success. */
  lf_ret_t (*remove)(EventQueue* self, AbstractEvent* event);

  size_t size;        /**< @brief Current number of events in the queue. */
  size_t capacity;    /**< @brief Maximum number of events the queue can hold. */
  ArbitraryEvent* array; /**< @brief Backing array of the event queue. */
  MUTEX_T mutex;      /**< @brief Mutex protecting concurrent access. */
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
