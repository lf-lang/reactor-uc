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
  void (*heapify_locked)(EventQueue* self, size_t idx);

  size_t size;
  size_t capacity;
  ArbitraryEvent* array;
  MUTEX_T mutex;
};

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
