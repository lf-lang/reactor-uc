#include "reactor-uc/queues.h"
#include "assert.h"
#include "reactor-uc/logging.h"
#include <string.h>

static void swap(Event *ev1, Event *ev2) {
  Event temp = *ev2;
  *ev2 = *ev1;
  *ev1 = temp;
}
tag_t EventQueue_next_tag(EventQueue *self) {
  if (self->size > 0) {
    return self->array[0].tag;
  }
  return FOREVER_TAG;
}

lf_ret_t EventQueue_insert(EventQueue *self, Event *event) {
  LF_DEBUG(QUEUE, "Inserting event with tag %" PRId64 ":%" PRIu32 " into EventQueue", event->tag.time,
           event->tag.microstep);
  if (self->size >= EVENT_QUEUE_SIZE) {
    LF_ERR(QUEUE, "EventQueue is full");
    return LF_OUT_OF_BOUNDS;
  }

  memcpy(&self->array[self->size], event, sizeof(Event));

  if (self->size++ > 0) {
    for (int i = ((int)self->size) / 2 - 1; i >= 0; i--) {
      self->heapify(self, i);
    }
  }
  return LF_OK;
}

void EventQueue_heapify(EventQueue *self, size_t idx) {
  LF_DEBUG(QUEUE, "Heapifying EventQueue at index %d", idx);
  // Find the smallest among root, left child and right child
  size_t smallest = idx;
  size_t left = 2 * idx + 1;
  size_t right = 2 * idx + 2;

  if (left < self->size && (lf_tag_compare(self->array[left].tag, self->array[smallest].tag) < 0)) {
    smallest = left;
  }
  if (right < self->size && (lf_tag_compare(self->array[right].tag, self->array[smallest].tag) < 0)) {
    smallest = right;
  }

  // Swap and continue heapifying if root is not smallest
  if (smallest != idx) {
    swap(&self->array[idx], &self->array[smallest]);
    self->heapify(self, smallest);
  }
}

Event EventQueue_pop(EventQueue *self) {
  LF_DEBUG(QUEUE, "Popping event from EventQueue");
  if (self->size == 0) {
    LF_ERR(QUEUE, "EventQueue is empty");
    validaten(false);
  }
  Event ret = self->array[0];
  swap(&self->array[0], &self->array[self->size - 1]);
  self->size--;
  for (int i = ((int)self->size) / 2 - 1; i >= 0; i--) {
    self->heapify(self, i);
  }
  return ret;
}

bool EventQueue_empty(EventQueue *self) { return self->size == 0; }
void EventQueue_ctor(EventQueue *self) {
  self->insert = EventQueue_insert;
  self->pop = EventQueue_pop;
  self->empty = EventQueue_empty;
  self->heapify = EventQueue_heapify;
  self->next_tag = EventQueue_next_tag;
  self->size = 0;
}

lf_ret_t ReactionQueue_insert(ReactionQueue *self, Reaction *reaction) {
  validate(reaction);
  validate(reaction->level < REACTION_QUEUE_SIZE);
  validate(reaction->level >= 0);
  validate(self->level_size[reaction->level] < REACTION_QUEUE_SIZE);
  validate(self->curr_level <= reaction->level);

  self->array[reaction->level][self->level_size[reaction->level]++] = reaction;
  if (reaction->level > self->max_active_level) {
    self->max_active_level = reaction->level;
  }
  return LF_OK;
}

Reaction *ReactionQueue_pop(ReactionQueue *self) {
  Reaction *ret = NULL;
  // Check if we can fetch a new reaction from same level
  if (self->level_size[self->curr_level] > self->curr_index) {
    ret = self->array[self->curr_level][self->curr_index];
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

bool ReactionQueue_empty(ReactionQueue *self) {
  if (self->max_active_level < 0 || self->curr_level > self->max_active_level) {
    return true;
  }
  if (self->curr_level == self->max_active_level && self->curr_index >= self->level_size[self->curr_level]) {
    return true;
  }
  return false;
}

void ReactionQueue_reset(ReactionQueue *self) {
  self->curr_index = 0;
  self->curr_level = 0;
  for (int i = 0; i <= self->max_active_level; i++) {
    self->level_size[i] = 0;
  }
  self->max_active_level = -1;
}

void ReactionQueue_ctor(ReactionQueue *self) {
  self->insert = ReactionQueue_insert;
  self->pop = ReactionQueue_pop;
  self->empty = ReactionQueue_empty;
  self->reset = ReactionQueue_reset;
  self->curr_index = 0;
  self->curr_level = 0;
  self->max_active_level = -1;
  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    self->level_size[i] = 0;
    for (int j = 0; j < REACTION_QUEUE_SIZE; j++) {
      self->array[i][j] = NULL;
    }
  }
}
