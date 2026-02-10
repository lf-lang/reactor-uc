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

struct EventQueue {
  tag_t (*next_tag)(EventQueue* self);
  lf_ret_t (*insert)(EventQueue* self, AbstractEvent* event);
  lf_ret_t (*pop)(EventQueue* self, AbstractEvent* event);
  bool (*empty)(EventQueue* self);
  void (*build_heap)(EventQueue* self);
  void (*heapify)(EventQueue* self, size_t idx);
  ArbitraryEvent* (*find_equal_same_tag)(EventQueue* self, AbstractEvent* event);
  lf_ret_t (*remove)(EventQueue* self, AbstractEvent* event);

  size_t size;
  size_t capacity;
  ArbitraryEvent* array;
  MUTEX_T mutex;
};

static inline size_t lchild_idx(size_t parent_idx) { return (parent_idx * 2) + 1; }
static inline size_t rchild_idx(size_t parent_idx) { return lchild_idx(parent_idx) + 1; }
static inline size_t parent_idx(size_t child_idx) { return (child_idx - 1) / 2; }

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
