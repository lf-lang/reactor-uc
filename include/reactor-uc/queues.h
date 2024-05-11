#ifndef REACTOR_UC_QUEUES_H
#define REACTOR_UC_QUEUES_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/trigger.h"

typedef struct EventQueue EventQueue;
typedef struct ReactionQueue ReactionQueue;

struct EventQueue {
  tag_t (*next_tag)(EventQueue *self);
  void (*insert)(EventQueue *self, Trigger *trigger, tag_t tag);
  Trigger *(*pop)(EventQueue *self, tag_t *tag);
  bool (*empty)(EventQueue *self);

  Trigger *q;
  tag_t tag;
};

void EventQueue_ctor(EventQueue *self);

struct ReactionQueue {
  void (*insert)(ReactionQueue *self, Reaction *reaction);
  Reaction *(*pop)(ReactionQueue *self);
  bool (*empty)(ReactionQueue *self);

  Reaction *q;
};

void ReactionQueue_ctor(ReactionQueue *self);

#endif
