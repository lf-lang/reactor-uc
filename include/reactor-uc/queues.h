#ifndef REACTOR_UC_QUEUES_H
#define REACTOR_UC_QUEUES_H

// FIXME: These defines should be someplace else
#ifndef EVENT_QUEUE_SIZE
#define EVENT_QUEUE_SIZE 10
#endif

#ifndef REACTION_QUEUE_SIZE
#define REACTION_QUEUE_SIZE 10
#endif

#include "reactor-uc/event.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/trigger.h"

typedef struct EventQueue EventQueue;
typedef struct ReactionQueue ReactionQueue;

struct EventQueue {
  tag_t (*next_tag)(EventQueue *self);
  void (*insert)(EventQueue *self, Event event);
  Event (*pop)(EventQueue *self);
  bool (*empty)(EventQueue *self);
  void (*heapify)(EventQueue *self, size_t idx);

  Event array[EVENT_QUEUE_SIZE];
  size_t size;
};

void EventQueue_ctor(EventQueue *self);

struct ReactionQueue {
  void (*insert)(ReactionQueue *self, Reaction *reaction);
  Reaction *(*pop)(ReactionQueue *self);
  bool (*empty)(ReactionQueue *self);
  void (*reset)(ReactionQueue *self);

  int level_size[REACTION_QUEUE_SIZE];
  int curr_level;
  int max_active_level;
  int curr_index;
  Reaction *array[REACTION_QUEUE_SIZE][REACTION_QUEUE_SIZE];
};

void ReactionQueue_ctor(ReactionQueue *self);

#endif
