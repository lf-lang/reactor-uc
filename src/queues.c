#include "reactor-uc/queues.h"
#include "assert.h"
#include "reactor-uc/reaction.h"

static void swap(Event *ev1, Event *ev2) {
  Event temp = *ev2;
  *ev2 = *ev1;
  *ev1 = temp;
}
tag_t EventQueue_next_tag(EventQueue *self) {
  if (self->size_ > 0) {
    return self->array_[0].tag;
  }
  return FOREVER_TAG;
}

QueueReply EventQueue_insert(EventQueue *self, Event event) {
  assert(self->size_ < EVENT_QUEUE_SIZE); // debug check

  if (self->size_ >= EVENT_QUEUE_SIZE) {
    return FULL;
  }

  if (self->size_ == 0) {
    self->array_[0] = event;
    self->size_++;
  } else {
    self->array_[self->size_] = event;
    self->size_++;
    for (int i = ((int)self->size_) / 2 - 1; i >= 0; i--) {
      self->heapify(self, i);
    }
  }

  return INSERTED;
}

void EventQueue_heapify(EventQueue *self, size_t idx) {
  assert(self->size_ > 1);
  // Find the largest among root, left child and right child
  size_t largest = idx;
  size_t left = 2 * idx + 1;
  size_t right = 2 * idx + 2;
  if (left < self->size_ && (lf_tag_compare(self->array_[largest].tag, self->array_[left].tag) > 0)) {
    largest = left;
  }
  if (right < self->size_ && (lf_tag_compare(self->array_[largest].tag, self->array_[right].tag) > 0)) {
    largest = right;
  }

  // Swap and continue heapifying if root is not largest
  if (largest != idx) {
    swap(&self->array_[idx], &self->array_[largest]);
    self->heapify(self, largest);
  }
}

Event EventQueue_pop(EventQueue *self) {
  Event ret = self->array_[0];
  swap(&self->array_[0], &self->array_[self->size_ - 1]);
  self->size_--;
  for (int i = ((int)self->size_) / 2 - 1; i >= 0; i--) {
    self->heapify(self, i);
  }
  return ret;
}

bool EventQueue_full(EventQueue *self) { return self->size_ >= EVENT_QUEUE_SIZE; }

bool EventQueue_empty(EventQueue *self) { return self->size_ == 0; }
void EventQueue_ctor(EventQueue *self) {
  self->insert = EventQueue_insert;
  self->pop = EventQueue_pop;
  self->empty = EventQueue_empty;
  self->heapify = EventQueue_heapify;
  self->next_tag = EventQueue_next_tag;
  self->size_ = 0;
}

void ReactionQueue_insert(ReactionQueue *self, Reaction *reaction) {
  assert(reaction);
  assert(reaction->level <= REACTION_QUEUE_SIZE);
  assert(self->level_size_[reaction->level] < REACTION_QUEUE_SIZE);
  assert(reaction->level < REACTION_QUEUE_SIZE);
  assert(self->level_size_[reaction->level] < REACTION_QUEUE_SIZE);

  int level = reaction->level;

  // potential bounds check level < ARRAY_SIZE
  self->array_[level][self->level_size_[level]++] = reaction;
  if (level > self->max_active_level_) {
    self->max_active_level_ = level;
  }
}

Reaction *ReactionQueue_pop(ReactionQueue *self) {
  Reaction *ret = NULL;

  if (self->level_size_[self->current_level_] > self->current_index_) {
    // Check if we can fetch a new reaction from same level

    ret = self->array_[self->current_level_][self->current_index_];
    self->current_index_++;
  } else if (self->current_level_ < self->max_active_level_) {
    // fetch reaction from the next level

    self->current_level_++;
    self->current_index_ = 0;
    ret = ReactionQueue_pop(self);
  } else {
    // queue is empty return NULL

    ret = NULL;
  }
  return ret;
}

bool ReactionQueue_empty(ReactionQueue *self) {
  if (self->max_active_level_ < 0 || self->current_level_ > self->max_active_level_) {
    return true;
  }
  if (self->current_level_ == self->max_active_level_ &&
      self->current_index_ >= self->level_size_[self->current_level_]) {
    return true;
  }
  return false;
}

void ReactionQueue_reset(ReactionQueue *self) {
  self->current_index_ = 0;
  self->current_level_ = 0;
  for (int i = 0; i <= self->max_active_level_; i++) {
    self->level_size_[i] = 0;
  }
  self->max_active_level_ = -1;
}

void ReactionQueue_ctor(ReactionQueue *self) {
  self->insert = ReactionQueue_insert;
  self->pop = ReactionQueue_pop;
  self->empty = ReactionQueue_empty;
  self->reset = ReactionQueue_reset;
  self->current_index_ = 0;
  self->current_level_ = 0;
  self->max_active_level_ = -1;
  for (int i = 0; i < REACTION_QUEUE_SIZE; i++) {
    self->level_size_[i] = 0;
  }
}
