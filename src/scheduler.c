#include "reactor-uc/environment.h"
#include "reactor-uc/reactor-uc.h"
#include "reactor-uc/scheduler.h"

static void reset_is_present_recursive(Reactor *reactor) {
  for (size_t i = 0; i < reactor->triggers_size; i++) {
    reactor->triggers[i]->is_present = false;
  }
  for (size_t i = 0; i < reactor->children_size; i++) {
    reset_is_present_recursive(reactor->children[i]);
  }
}

void Scheduler_prepare_timestep(Scheduler *self) {
  self->reaction_queue.reset(&self->reaction_queue);

  // FIXME: Improve this expensive resetting of all `is_present` fields of triggers.

  Environment *env = self->env;
  reset_is_present_recursive(env->main);
}
void Scheduler_run(Scheduler *self) {
  while (!self->event_queue.empty(&self->event_queue)) {
    tag_t next_tag = self->event_queue.next_tag(&self->event_queue);
    self->env->wait_until(self->env, next_tag.time);
    do {
      self->prepare_timestep(self);
      Event event = self->event_queue.pop(&self->event_queue);
      self->env->current_tag = event.tag;
      event.trigger->is_present = true;
      if (event.trigger->update_value) {
        event.trigger->update_value(event.trigger);
      }

      for (size_t i = 0; i < event.trigger->effects_size; i++) {
        self->reaction_queue.insert(&self->reaction_queue, event.trigger->effects[i]);
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
  self->prepare_timestep = Scheduler_prepare_timestep;
  EventQueue_ctor(&self->event_queue);
  ReactionQueue_ctor(&self->reaction_queue);
}
