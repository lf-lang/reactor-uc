#ifndef REACTOR_UC_QUEUES_H
#define REACTOR_UC_QUEUES_H

#ifndef EVENT_QUEUE_SIZE
#define EVENT_QUEUE_SIZE 10
#endif

#ifndef REACTION_QUEUE_SIZE
#define REACTION_QUEUE_SIZE 10
#endif

#include "reactor-uc/event.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/trigger.h"

typedef struct EventQueue EventQueue;
typedef struct ReactionQueue ReactionQueue;

struct EventQueue {
  // methods
  tag_t (*next_tag)(EventQueue *self);
  void (*insert)(EventQueue *self, Event event);
  Event (*pop)(EventQueue *self);
  bool (*empty)(EventQueue *self);
  void (*heapify)(EventQueue *self, size_t idx);

  // struct members
  Event array_[EVENT_QUEUE_SIZE];
  size_t size_;
};

void EventQueue_ctor(EventQueue *self);

struct ReactionQueue {
  // methods
  void (*insert)(ReactionQueue *self, Reaction *reaction);
  Reaction *(*pop)(ReactionQueue *self);
  bool (*empty)(ReactionQueue *self);
  void (*reset)(ReactionQueue *self);

  // struct members
  int level_size_[REACTION_QUEUE_SIZE];
  int current_level_;
  int max_active_level_;
  int current_index_;
  Reaction *array_[REACTION_QUEUE_SIZE][REACTION_QUEUE_SIZE];
};

void ReactionQueue_ctor(ReactionQueue *self);

#endif
