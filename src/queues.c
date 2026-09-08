#include "reactor-uc/queues.h"
#include "assert.h"
#include "reactor-uc/logging.h"
#include <string.h>

/**
 * @brief Return the index of the left child of a node in a binary heap.
 * @param parent_idx Index of the parent node.
 * @return Index of the left child.
 */
static inline size_t lchild_idx(size_t parent_idx) { return (parent_idx * 2) + 1; }

/**
 * @brief Return the index of the right child of a node in a binary heap.
 * @param parent_idx Index of the parent node.
 * @return Index of the right child.
 */
static inline size_t rchild_idx(size_t parent_idx) { return lchild_idx(parent_idx) + 1; }

/**
 * @brief Return the index of the parent of a node in a binary heap.
 * @param child_idx Index of the child node.
 * @return Index of the parent.
 */
static inline size_t parent_idx(size_t child_idx) { return (child_idx - 1) / 2; }

static void swap(ArbitraryEvent* ev1, ArbitraryEvent* ev2) {
  ArbitraryEvent temp = *ev2;
  *ev2 = *ev1;
  *ev1 = temp;
}

static tag_t EventQueue_next_tag(EventQueue* self) {
  MUTEX_LOCK(self->mutex);
  tag_t ret = FOREVER_TAG;
  if (self->size > 0) {
    ret = get_tag(&self->array[0]);
  }
  MUTEX_UNLOCK(self->mutex);
  return ret;
}

static lf_ret_t EventQueue_insert(EventQueue* self, AbstractEvent* event) {
  LF_DEBUG(QUEUE, "Inserting event with tag " PRINTF_TAG " into EventQueue", event->tag);
  MUTEX_LOCK(self->mutex);
  if (self->size >= self->capacity) {
    LF_ERR(QUEUE, "EventQueue is full has size %zu", self->size);
    MUTEX_UNLOCK(self->mutex);
    return LF_EVENT_QUEUE_FULL;
  }

  size_t event_size;
  switch (event->type) {
  case EVENT:
    event_size = sizeof(Event);
    break;
  case SYSTEM_EVENT:
    event_size = sizeof(SystemEvent);
    break;
  default:
    LF_ERR(QUEUE, "Unknown event type %d", event->type);
    MUTEX_UNLOCK(self->mutex);
    return LF_ERR;
  }

  memcpy(&self->array[self->size], event, event_size);

  size_t idx = self->size++;
  tag_t event_tag = get_tag(&self->array[idx]);

  // Bubble up the newly added event
  while (idx > 0) {
    size_t p_idx = parent_idx(idx);
    if (lf_tag_compare(event_tag, get_tag(&self->array[p_idx])) >= 0) {
      break;
    }
    swap(&self->array[idx], &self->array[p_idx]);
    idx = p_idx;
  };

  MUTEX_UNLOCK(self->mutex);
  return LF_OK;
}

/**
 * @brief Restore the heap invariant downward from @p idx.
 * The caller must already hold `self->mutex`.
 */
static void heapify(EventQueue* self, size_t idx) {
  LF_DEBUG(QUEUE, "Heapifying EventQueue, starting at index %zu", idx);
  while (idx < self->size) {
    LF_DEBUG(QUEUE, "Heapifying EventQueue at index %zu", idx);
    size_t left = lchild_idx(idx);
    size_t right = rchild_idx(idx);
    size_t smallest = idx;

    if (left < self->size && (lf_tag_compare(get_tag(&self->array[left]), get_tag(&self->array[smallest])) < 0)) {
      smallest = left;
    }
    if (right < self->size && (lf_tag_compare(get_tag(&self->array[right]), get_tag(&self->array[smallest])) < 0)) {
      smallest = right;
    }
    if (smallest == idx) {
      break;
    }
    swap(&self->array[idx], &self->array[smallest]);
    idx = smallest;
  }
}

/**
 * @brief Restore the heap invariant across the whole queue.
 * The caller must already hold `self->mutex`.
 */
static void build_heap(EventQueue* self) {
  for (size_t i = self->size / 2; i-- > 0;) {
    heapify(self, i);
  }
}

static void EventQueue_heapify(EventQueue* self, size_t idx) {
  MUTEX_LOCK(self->mutex);
  heapify(self, idx);
  MUTEX_UNLOCK(self->mutex);
}

static void EventQueue_build_heap(EventQueue* self) {
  MUTEX_LOCK(self->mutex);
  build_heap(self);
  MUTEX_UNLOCK(self->mutex);
}

/** Shift all tags in the queue by the specified step. */
static void EventQueue_shift_all_tags(EventQueue* self, interval_t step) {
  MUTEX_LOCK(self->mutex);

  for (size_t i = 0; i < self->size; i++) {
    AbstractEvent* event = &self->array[i].event.super;
    instant_t new_tag = lf_time_add(event->tag.time, step);
    if (new_tag < 0) {
      new_tag = 0;
    }
    event->tag.time = new_tag;
  }
  build_heap(self);

  MUTEX_UNLOCK(self->mutex);
}

static int find_matching_event_idx(EventQueue* self, AbstractEvent* event,
                                   bool (*match_fn)(ArbitraryEvent*, ArbitraryEvent*)) {
  for (size_t i = 0; i < self->size; i++) {
    ArbitraryEvent* current_event = &self->array[i];

    if (match_fn(current_event, (ArbitraryEvent*)event)) {
      return i;
    }
  }
  return -1;
}

static int find_equal_same_tag_idx(EventQueue* self, AbstractEvent* event) {
  if (event->type == EVENT) {
    return find_matching_event_idx(self, event, events_same_tag_and_trigger);
  }

  validate(event->type == SYSTEM_EVENT);
  return find_matching_event_idx(self, event, events_same_tag_and_handler);
}

static ArbitraryEvent* EventQueue_find_equal_same_tag(EventQueue* self, AbstractEvent* event) {

  MUTEX_LOCK(self->mutex);
  int event_idx = find_equal_same_tag_idx(self, event);
  ArbitraryEvent* found = NULL;

  if (event_idx >= 0) {
    found = &self->array[event_idx];
  }

  MUTEX_UNLOCK(self->mutex);
  return found;
}

static void sift_up(EventQueue* self, size_t idx) {
  while (idx > 0) {
    size_t p_idx = parent_idx(idx);
    if (lf_tag_compare(get_tag(&self->array[idx]), get_tag(&self->array[p_idx])) >= 0) {
      break;
    }
    swap(&self->array[idx], &self->array[p_idx]);
    idx = p_idx;
  }
}

static lf_ret_t EventQueue_remove_matching(EventQueue* self, AbstractEvent* key, void** out_payload) {
  MUTEX_LOCK(self->mutex);
  int event_idx = find_equal_same_tag_idx(self, key);

  if (event_idx < 0) {
    MUTEX_UNLOCK(self->mutex);
    return LF_EVENT_NOT_FOUND;
  }

  if (out_payload != NULL) {
    *out_payload = self->array[event_idx].event.super.payload;
  }

  swap(&self->array[event_idx], &self->array[self->size - 1]);
  self->size--;

  // The relocated element may violate the heap in either direction.
  if (event_idx < (int)self->size) {
    heapify(self, event_idx); // downward
    sift_up(self, event_idx); // upward
  }
  MUTEX_UNLOCK(self->mutex);
  return LF_OK;
}

static lf_ret_t EventQueue_remove(EventQueue* self, AbstractEvent* event) {
  return EventQueue_remove_matching(self, event, NULL);
}

static lf_ret_t EventQueue_replace_payload(EventQueue* self, AbstractEvent* key, const void* new_value,
                                           size_t payload_size) {
  if (new_value == NULL) {
    return LF_INVALID_VALUE;
  }

  MUTEX_LOCK(self->mutex);
  int event_idx = find_equal_same_tag_idx(self, key);

  if (event_idx < 0) {
    MUTEX_UNLOCK(self->mutex);
    return LF_EVENT_NOT_FOUND;
  }

  void* payload = self->array[event_idx].event.super.payload;
  if (payload == NULL) {
    MUTEX_UNLOCK(self->mutex);
    return LF_INVALID_VALUE;
  }
  memcpy(payload, new_value, payload_size);

  MUTEX_UNLOCK(self->mutex);
  return LF_OK;
}

static lf_ret_t EventQueue_pop(EventQueue* self, AbstractEvent* event) {
  LF_DEBUG(QUEUE, "Popping event from EventQueue");
  MUTEX_LOCK(self->mutex);
  if (self->size == 0) {
    LF_ERR(QUEUE, "EventQueue is empty");
    MUTEX_UNLOCK(self->mutex);
    return LF_EVENT_QUEUE_EMPTY;
  }

  ArbitraryEvent ret = self->array[0];
  swap(&self->array[0], &self->array[self->size - 1]);
  self->size--;
  heapify(self, 0);

  size_t event_size;
  switch (ret.event.super.type) {
  case EVENT:
    event_size = sizeof(Event);
    break;
  case SYSTEM_EVENT:
    event_size = sizeof(SystemEvent);
    break;
  default:
    LF_ERR(QUEUE, "Unknown event type %d", ret.event.super.type);
    MUTEX_UNLOCK(self->mutex);
    return LF_ERR;
  }
  memcpy(event, &ret, event_size);

  MUTEX_UNLOCK(self->mutex);
  return LF_OK;
}

static bool EventQueue_empty(EventQueue* self) {
  MUTEX_LOCK(self->mutex);
  bool ret = self->size == 0;
  MUTEX_UNLOCK(self->mutex);
  return ret;
}

void EventQueue_ctor(EventQueue* self, ArbitraryEvent* array, size_t capacity) {
  self->insert = EventQueue_insert;
  self->pop = EventQueue_pop;
  self->empty = EventQueue_empty;
  self->build_heap = EventQueue_build_heap;
  self->heapify = EventQueue_heapify;
  self->shift_all_tags = EventQueue_shift_all_tags;
  self->find_equal_same_tag = EventQueue_find_equal_same_tag;
  self->remove = EventQueue_remove;
  self->remove_matching = EventQueue_remove_matching;
  self->replace_payload = EventQueue_replace_payload;
  self->next_tag = EventQueue_next_tag;
  self->size = 0;
  self->capacity = capacity;
  self->array = array;
  Mutex_ctor(&self->mutex.super);
}

static lf_ret_t ReactionQueue_insert(ReactionQueue* self, Reaction* reaction) {
  validate(reaction);
  validate(reaction->level < (int)self->capacity);
  validate(reaction->level >= 0);

  validate(self->curr_level <= reaction->level);

  // checking if the reaction to be inserted is already in the queue
  // e.g., when a reaction is triggered by two or more inputs.
  // This needs to be done before checking if the queue is full, because
  // if the reaction is already in the queue, we don't need to insert it again.
  const Reaction* const level_tail = self->level_tail[reaction->level];
  if (level_tail != NULL) {
    const Reaction* node = level_tail;
    do {
      node = node->_next_in_level;
      if (node == reaction) {
        return LF_OK;
      }
    } while (node != level_tail);
  }

  // Append, so a level is popped in insertion order.
  Reaction* tail = self->level_tail[reaction->level];
  if (tail == NULL) {
    reaction->_next_in_level = reaction;
  } else {
    reaction->_next_in_level = tail->_next_in_level;
    tail->_next_in_level = reaction;
  }
  self->level_tail[reaction->level] = reaction;

  if (reaction->level > self->max_active_level) {
    self->max_active_level = reaction->level;
  }
  if (self->min_active_level < 0 || reaction->level < self->min_active_level) {
    self->min_active_level = reaction->level;
  }

  self->level_occupied[reaction->level / LF_LEVEL_WORD_BITS] |= ((lf_level_word_t)1)
                                                                << (reaction->level % LF_LEVEL_WORD_BITS);
  self->remaining++;
  return LF_OK;
}

// The next reaction still to run at `curr_level`, or NULL. Derived on each
// call so that a reaction appended to this level WHILE it is being drained is
// seen.
static Reaction* ReactionQueue_level_next(const ReactionQueue* self) {
  if (self->curr_level < 0) {
    return NULL;
  }
  Reaction* const tail = self->level_tail[self->curr_level];
  if (tail == NULL) {
    return NULL;
  }
  if (self->curr_last == NULL) {
    return tail->_next_in_level;
  }
  // Having just returned the tail means the level is drained.
  return self->curr_last == tail ? NULL : self->curr_last->_next_in_level;
}

// The lowest occupied level at or above `from`, or -1.
static int ReactionQueue_next_occupied(const ReactionQueue* self, int from) {
  if (from < 0) {
    from = 0;
  }
  if (self->max_active_level < 0 || from > self->max_active_level) {
    return -1;
  }
  int word_idx = from / LF_LEVEL_WORD_BITS;
  const int last_word = self->max_active_level / LF_LEVEL_WORD_BITS;
  // Mask off the bits below `from`, so the first word only reports levels at
  // or above it.
  lf_level_word_t word = self->level_occupied[word_idx] & (~(lf_level_word_t)0 << (from % LF_LEVEL_WORD_BITS));
  while (word == 0) {
    if (++word_idx > last_word) {
      return -1;
    }
    word = self->level_occupied[word_idx];
  }
  const int level = word_idx * LF_LEVEL_WORD_BITS + lf_level_word_ctz(word);
  return level > self->max_active_level ? -1 : level;
}

static Reaction* ReactionQueue_pop(ReactionQueue* self) {
  if (self->remaining == 0) {
    return NULL;
  }
  while (1) {
    Reaction* next_at_level = ReactionQueue_level_next(self);
    // If there is a reaction at the current level, return it and advance the
    // cursor. Otherwise, advance to the next occupied level and try again.
    if (next_at_level != NULL) {
      self->curr_last = next_at_level;
      self->remaining--;
      return next_at_level;
    }
    const int next = ReactionQueue_next_occupied(self, self->curr_level + 1);
    // No more reactions at or above the current level, so the queue is empty.
    if (next < 0) {
      self->curr_level = self->max_active_level + 1;
      return NULL;
    }
    // Advance to the next occupied level and reset the cursor for that level.
    self->curr_level = next;
    self->curr_last = NULL;
  }
}

static bool ReactionQueue_empty(ReactionQueue* self) { return self->remaining == 0; }

static void ReactionQueue_reset(ReactionQueue* self) {
  self->curr_level = -1;
  self->curr_last = NULL;
  self->remaining = 0;
  if (self->min_active_level >= 0) {
    // Clear only the levels that were actually written. The bitmap already
    // names them, so this costs one word test per word of levels plus one
    // store per occupied level, rather than a store per level between the
    // lowest and the highest.
    int word = self->min_active_level / LF_LEVEL_WORD_BITS;
    while (word <= self->max_active_level / LF_LEVEL_WORD_BITS) {
      lf_level_word_t bits = self->level_occupied[word];
      while (bits != 0) {
        self->level_tail[word * LF_LEVEL_WORD_BITS + lf_level_word_ctz(bits)] = NULL;
        // Clears the lowest set bit.
        bits &= bits - 1;
      }
      self->level_occupied[word] = 0;
      word++;
    }
  }
  self->max_active_level = -1;
  self->min_active_level = -1;
}

void ReactionQueue_ctor(ReactionQueue* self, Reaction** level_tail, lf_level_word_t* level_occupied, size_t capacity) {
  self->insert = ReactionQueue_insert;
  self->pop = ReactionQueue_pop;
  self->empty = ReactionQueue_empty;
  self->reset = ReactionQueue_reset;
  self->curr_last = NULL;
  self->curr_level = -1;
  self->remaining = 0;
  self->max_active_level = -1;
  self->min_active_level = -1;
  self->level_occupied = level_occupied;
  for (size_t i = 0; i < LF_LEVEL_WORDS(capacity); i++) {
    self->level_occupied[i] = 0;
  }
  self->capacity = capacity;
  self->level_tail = level_tail;
  for (size_t i = 0; i < capacity; i++) {
    self->level_tail[i] = NULL;
  }
}
