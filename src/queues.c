#include "assert.h"
#include "reactor-uc/queues.h"

tag_t EventQueue_next_tag(EventQueue *self) { return self->tag; }
void EventQueue_insert(EventQueue *self, Trigger *trigger, tag_t tag) {
  self->tag = tag;
  self->q = trigger;
}

Trigger *EventQueue_pop(EventQueue *self, tag_t *tag) {
  Trigger *ret = NULL;
  if (self->q) {
    *tag = self->tag;
    ret = self->q;
    self->q = NULL;
    self->tag = NEVER_TAG;
  }
  return ret;
}

bool EventQueue_empty(EventQueue *self) { return self->q == NULL; }
void EventQueue_ctor(EventQueue *self) {
  self->q = NULL;
  self->tag = NEVER_TAG;
  self->next_tag = EventQueue_next_tag;
  self->insert = EventQueue_insert;
  self->pop = EventQueue_pop;
  self->empty = EventQueue_empty;
}

void ReactionQueue_insert(ReactionQueue *self, Reaction *reaction) {
  assert(reaction);
  self->q = reaction;
}
Reaction *ReactionQueue_pop(ReactionQueue *self) {
  Reaction *ret = NULL;
  if (self->q) {
    ret = self->q;
    self->q = NULL;
  }
  return ret;
}

bool ReactionQueue_empty(ReactionQueue *self) { return self->q == NULL; }

void ReactionQueue_ctor(ReactionQueue *self) {
  self->insert = ReactionQueue_insert;
  self->pop = ReactionQueue_pop;
  self->empty = ReactionQueue_empty;
}
