#include "reactor-uc/queues.h"
#include "assert.h"
#include "reactor-uc/logging.h"
#include <string.h>

#define GET_TAG(arbitrary_event) (arbitrary_event).event.super.tag
#define ACCESS(arr, size, row, col) (arr)[(row) * (size) + (col)]

static void swap(ArbitraryEvent* ev1, ArbitraryEvent* ev2) {
  ArbitraryEvent temp = *ev2;
  *ev2 = *ev1;
  *ev1 = temp;
}
static tag_t EventQueue_next_tag(EventQueue* self) {
  MUTEX_LOCK(self->mutex);
  tag_t ret = FOREVER_TAG;
  if (self->size > 0) {
    ret = GET_TAG(self->array[0]);
  }
  MUTEX_UNLOCK(self->mutex);
  return ret;
}

static lf_ret_t EventQueue_insert(EventQueue* self, AbstractEvent* event) {
  LF_DEBUG(QUEUE, "Inserting event with tag " PRINTF_TAG " into EventQueue", event->tag);
  MUTEX_LOCK(self->mutex);
  if (self->size >= self->capacity) {
    LF_ERR(QUEUE, "EventQueue is full has size %d", self->size);
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

  if (self->size++ > 0) {
    for (int i = ((int)self->size) / 2 - 1; i >= 0; i--) {
      self->heapify_locked(self, i);
    }
  }

  MUTEX_UNLOCK(self->mutex);
  return LF_OK;
}

static void EventQueue_heapify_locked(EventQueue* self, size_t idx) {
  LF_DEBUG(QUEUE, "Heapifying EventQueue at index %d", idx);
  // Find the smallest among root, left child and right child
  size_t smallest = idx;
  size_t left = 2 * idx + 1;
  size_t right = 2 * idx + 2;

  if (left < self->size && (lf_tag_compare(GET_TAG(self->array[left]), GET_TAG(self->array[smallest])) < 0)) {
    smallest = left;
  }
  if (right < self->size && (lf_tag_compare(GET_TAG(self->array[right]), GET_TAG(self->array[smallest])) < 0)) {
    smallest = right;
  }

  // Swap and continue heapifying if root is not smallest
  if (smallest != idx) {
    swap(&self->array[idx], &self->array[smallest]);
    self->heapify_locked(self, smallest);
  }
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
  for (int i = ((int)self->size) / 2 - 1; i >= 0; i--) {
    self->heapify_locked(self, i);
  }
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

static bool EventQueue_empty(EventQueue* self) { return self->size == 0; }

void EventQueue_ctor(EventQueue* self, ArbitraryEvent* array, size_t capacity) {
  self->insert = EventQueue_insert;
  self->pop = EventQueue_pop;
  self->empty = EventQueue_empty;
  self->heapify_locked = EventQueue_heapify_locked;
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
  for (int i = 0; i < self->level_size[reaction->level]; i++) {
    if (ACCESS(self->array, self->capacity, reaction->level, i) == reaction) {
      return LF_OK;
    }
  }

  // now we can check if the queue is full
  validate(self->level_size[reaction->level] < (int)self->capacity);

  ACCESS(self->array, self->capacity, reaction->level, self->level_size[reaction->level]) = reaction;
  self->level_size[reaction->level]++;
  if (reaction->level > self->max_active_level) {
    self->max_active_level = reaction->level;
  }
  return LF_OK;
}

static Reaction* ReactionQueue_pop(ReactionQueue* self) {
  Reaction* ret = NULL;
  // Check if we can fetch a new reaction from same level
  if (self->level_size[self->curr_level] > self->curr_index) {
    ret = ACCESS(self->array, self->capacity, self->curr_level, self->curr_index);
    self->curr_index++;
  } else if (self->curr_level < self->max_active_level) {
    self->curr_level++;
    self->curr_index = 0;
    ret = ReactionQueue_pop(self);
  } else {
    ret = NULL;
  }
  return ret;
}

static bool ReactionQueue_empty(ReactionQueue* self) {
  if (self->max_active_level < 0 || self->curr_level > self->max_active_level) {
    return true;
  }
  if (self->curr_level == self->max_active_level && self->curr_index >= self->level_size[self->curr_level]) {
    return true;
  }
  return false;
}

static void ReactionQueue_reset(ReactionQueue* self) {
  self->curr_index = 0;
  self->curr_level = 0;
  for (int i = 0; i <= self->max_active_level; i++) {
    self->level_size[i] = 0;
  }
  self->max_active_level = -1;
}

void ReactionQueue_ctor(ReactionQueue* self, Reaction** array, int* level_size, size_t capacity) {
  self->insert = ReactionQueue_insert;
  self->pop = ReactionQueue_pop;
  self->empty = ReactionQueue_empty;
  self->reset = ReactionQueue_reset;
  self->curr_index = 0;
  self->curr_level = 0;
  self->max_active_level = -1;
  self->capacity = capacity;
  self->level_size = level_size;
  self->array = array;
  for (size_t i = 0; i < capacity; i++) {
    self->level_size[i] = 0;
    for (size_t j = 0; j < capacity; j++) {
      ACCESS(self->array, self->capacity, i, j) = NULL;
    }
  }
}
