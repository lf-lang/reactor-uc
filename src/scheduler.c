#include "reactor-uc/environment.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/scheduler.h"

void Scheduler_run(Scheduler *self) {
  while (!self->event_queue.empty(&self->event_queue)) {
    tag_t next_tag = self->event_queue.next_tag(&self->event_queue);
    self->env->wait_until(self->env, next_tag.time);
    do {
      tag_t popped_event_tag;
      Trigger *trigger = self->event_queue.pop(&self->event_queue, &popped_event_tag);
      self->env->current_tag = popped_event_tag;
      if (trigger->update_value) {
        trigger->update_value(trigger);
      }

      for (size_t i = 0; i < trigger->effects_size; i++) {
        self->reaction_queue.insert(&self->reaction_queue, trigger->effects[i]);
      }
    } while (lf_tag_compare(next_tag, self->event_queue.next_tag(&self->event_queue)) == 0);

    while (!self->reaction_queue.empty(&self->reaction_queue)) {
      Reaction *reaction = self->reaction_queue.pop(&self->reaction_queue);
      reaction->body(reaction);
    }
  }
  printf("Scheduler out of events. Shutting down.\n");
}

void Scheduler_ctor(Scheduler *self, Environment *env) {
  self->env = env;
  self->run = Scheduler_run;
  EventQueue_ctor(&self->event_queue);
  ReactionQueue_ctor(&self->reaction_queue);
}
