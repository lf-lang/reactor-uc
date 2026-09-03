#ifndef REACTOR_UC_QUEUES_H
#define REACTOR_UC_QUEUES_H

#include <stdint.h>
#include "reactor-uc/error.h"
#include "reactor-uc/event.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/platform.h"

typedef struct EventQueue EventQueue;
typedef struct ReactionQueue ReactionQueue;

/**
 * @brief One machine word of the reaction queue's level-occupancy bitmap.
 *
 * The bitmap holds one bit per level, rounded up to a whole number of words, so
 * it occupies `LF_LEVEL_WORDS(capacity) * sizeof(lf_level_word_t)` bytes. What
 * the width chosen here decides is how many levels a single test covers while
 * scanning for the next occupied one.
 *
 * Chosen from `UINTPTR_MAX`: in general a word wider than the target's registers
 * performs worse.
 */
#if defined(UINTPTR_MAX) && UINTPTR_MAX <= 0xFFFFFFFFu
typedef uint32_t lf_level_word_t;
#else
typedef uint64_t lf_level_word_t;
#endif

#define LF_LEVEL_WORD_BITS ((int)(sizeof(lf_level_word_t) * 8))
// One bit per level for `capacity` levels.
#define LF_LEVEL_WORDS(capacity) (((capacity) + LF_LEVEL_WORD_BITS - 1) / LF_LEVEL_WORD_BITS)

/** @brief Index of the lowest set bit. `word` must be non-zero. */
static inline int lf_level_word_ctz(lf_level_word_t word) {
  if (sizeof(lf_level_word_t) == sizeof(unsigned int)) {
    return __builtin_ctz((unsigned int)word);
  }
  return __builtin_ctzll((unsigned long long)word);
}

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
  /** @brief Remove the event equal to @p event from the queue. Returns
   * LF_EVENT_NOT_FOUND if no such event exists, LF_OK otherwise.
   */
  lf_ret_t (*remove)(EventQueue* self, AbstractEvent* event);

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

  // Per-level FIFOs, threaded through the reactions themselves.
  Reaction** level_tail;
  // The last reaction returned from `curr_level`, or NULL before the first one.
  Reaction* curr_last;
  int curr_level;
  // Reactions inserted this tag and not yet popped. `empty` is asked once per
  // dispatch, so it has to be O(1).
  size_t remaining;
  // Lowest and highest level holding a reaction this tag. -1/-1 when empty.
  // The pair bounds every walk over the level arrays, so both popping and
  // resetting cost what was actually inserted rather than the program's
  // level COUNT.
  int min_active_level;
  int max_active_level;
  // One bit per level, set while that level holds a reaction. `pop` finds the
  // next occupied level with a single `ctz` per LF_LEVEL_WORD_BITS levels
  // instead of stepping through the empty ones.
  lf_level_word_t* level_occupied;
  size_t capacity;
};

void ReactionQueue_ctor(ReactionQueue* self, Reaction** level_tail, lf_level_word_t* level_occupied, size_t capacity);

#endif
