#include "reactor-uc/scheduler.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/reactor-uc.h"

static void reset_is_present_recursive(Reactor *reactor) {
  for (size_t i = 0; i < reactor->triggers_size; i++) {
    reactor->triggers[i]->is_present = false;
  }
  for (size_t i = 0; i < reactor->children_size; i++) {
    reset_is_present_recursive(reactor->children[i]);
  }
}

void Scheduler_prepare_timestep(Scheduler *self) {
  self->reaction_queue_.reset(&self->reaction_queue_);

  // FIXME: Improve this expensive resetting of all `is_present` fields of triggers.

  Environment *env = self->env_;
  reset_is_present_recursive(env->main);
}

void Scheduler_run(Scheduler *self) {
  while (!self->event_queue_.empty(&self->event_queue_)) {
    // fetch tag from next event in queue
    tag_t next_tag = self->event_queue_.next_tag(&self->event_queue_);

    // sleep until this time
    self->env_->wait_until(self->env_, next_tag.time);

    // process this tag
    do {
      self->prepare_time_step(self);
      Event event = self->event_queue_.pop(&self->event_queue_);
      self->env_->current_tag = event.tag;
      event.trigger->is_present = true;

      if (event.trigger->update_value) {
        // e.g. Timer Event
        event.trigger->update_value(event.trigger);
      }

      for (size_t i = 0; i < event.trigger->effects_size; i++) {
        self->reaction_queue_.insert(&self->reaction_queue_, event.trigger->effects[i]);
      }

    } while (lf_tag_compare(next_tag, self->event_queue_.next_tag(&self->event_queue_)) == 0);

    while (!self->reaction_queue_.empty(&self->reaction_queue_)) {
      Reaction *reaction = self->reaction_queue_.pop(&self->reaction_queue_);
      reaction->body(reaction);
    }
  }
  printf("Scheduler out of events. Shutting down.\n");
}

void Scheduler_recursive_level(Reactor *reactor, int current_level) {

  for (unsigned int i = 0; i < reactor->children_size; i++) {
    Reactor *child_reactor = reactor->children[i];
    Scheduler_recursive_level(child_reactor, current_level);
  }
}

void Scheduler_ctor(Scheduler *self, Environment *env) {
  self->env_ = env;
  self->run = Scheduler_run;
  self->prepare_time_step = Scheduler_prepare_timestep;
  EventQueue_ctor(&self->event_queue_);
  ReactionQueue_ctor(&self->reaction_queue_);
}
