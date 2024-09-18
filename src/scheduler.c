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

void Scheduler_prepare_timestep(Scheduler *self) { self->reaction_queue.reset(&self->reaction_queue); }

void Scheduler_trigger_reactions(Scheduler *self, Trigger *trigger) {
  for (size_t i = 0; i < trigger->effects_size; i++) {
    self->reaction_queue.insert(&self->reaction_queue, trigger->effects[i]);
  }
}

void Scheduler_run_timestep(Scheduler *self) {
  while (!self->reaction_queue.empty(&self->reaction_queue)) {
    Reaction *reaction = self->reaction_queue.pop(&self->reaction_queue);
    reaction->body(reaction);
  }
}

void Scheduler_terminate(Scheduler *self) {
  printf("Scheduler terminating\n");
  self->reaction_queue.reset(&self->reaction_queue);
  Trigger *shutdown = &self->env->shutdown->super;
  while (shutdown) {
    self->trigger_reactions(self, shutdown);
    shutdown = shutdown->next;
  }
  self->run_timestep(self);
}

void Scheduler_clean_up_timestep(Scheduler *self) {
  // TODO: Improve this expensive resetting of all `is_present` fields of triggers.
  Environment *env = self->env;
  reset_is_present_recursive(env->main);
}
void Scheduler_run(Scheduler *self) {
  bool do_shutdown = false;
  while (!self->event_queue.empty(&self->event_queue)) {
    tag_t next_tag = self->event_queue.next_tag(&self->event_queue);
    if (lf_tag_compare(next_tag, self->env->stop_tag) > 0) {
      next_tag = self->env->stop_tag;
      do_shutdown = true;
    }
    self->env->wait_until(self->env, next_tag.time);
    if (do_shutdown)
      break;

    do {
      self->prepare_timestep(self);
      Event event = self->event_queue.pop(&self->event_queue);
      self->env->current_tag = event.tag;

      Trigger *trigger = event.trigger;
      do {
        trigger->is_present = true;
        if (trigger->update_value) {
          trigger->update_value(event.trigger);
        }
        self->trigger_reactions(self, trigger);

        trigger = trigger->next;
      } while (trigger);
    } while (lf_tag_compare(next_tag, self->event_queue.next_tag(&self->event_queue)) == 0);

    self->run_timestep(self);
    self->clean_up_timestep(self);
  }

  self->terminate(self);
}

void Scheduler_ctor(Scheduler *self, Environment *env) {
  self->env = env;
  self->run = Scheduler_run;
  self->prepare_timestep = Scheduler_prepare_timestep;
  self->clean_up_timestep = Scheduler_clean_up_timestep;
  self->trigger_reactions = Scheduler_trigger_reactions;
  self->run_timestep = Scheduler_run_timestep;
  self->terminate = Scheduler_terminate;
  EventQueue_ctor(&self->event_queue);
  ReactionQueue_ctor(&self->reaction_queue);
}
