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

void Scheduler_prepare_timestep(Scheduler *self, tag_t tag) {
  self->env->current_tag = tag;
  self->executing_tag = true;
  self->reaction_queue.reset(&self->reaction_queue);
}

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
  Environment *env = self->env;
  printf("Scheduler terminating\n");
  self->reaction_queue.reset(&self->reaction_queue);

  if (env->has_physical_action) {
    env->platform->leave_critical_section(env->platform);
  }

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
  self->executing_tag = false;
  reset_is_present_recursive(env->main);
}

// TODO: Reduce cognetive complexity of this function
void Scheduler_run(Scheduler *self) {
  Environment *env = self->env;
  int res = 0;
  bool do_shutdown = false;
  bool keep_alive = env->keep_alive || env->has_physical_action;

  if (env->has_physical_action) {
    env->platform->enter_critical_section(env->platform);
  }

  while (keep_alive || !self->event_queue.empty(&self->event_queue)) {
    tag_t next_tag = self->event_queue.next_tag(&self->event_queue);
    if (lf_tag_compare(next_tag, self->env->stop_tag) > 0) {
      next_tag = self->env->stop_tag;
      do_shutdown = true;
    } else {
      do_shutdown = false;
    }

    res = self->env->wait_until(self->env, next_tag.time);
    assert(res >= 0);
    if (res > 0) {
      continue;
    }

    if (do_shutdown) {
      break;
    }

    self->prepare_timestep(self, next_tag);

    do {
      Event event = self->event_queue.pop(&self->event_queue);

      Trigger *trigger = event.trigger;
      do {
        assert(trigger->is_scheduled);
        trigger->is_present = true;
        trigger->is_scheduled = false;
        if (trigger->update_value) {
          trigger->update_value(event.trigger);
        }
        self->trigger_reactions(self, trigger);

        trigger = trigger->next;
      } while (trigger);
    } while (lf_tag_compare(next_tag, self->event_queue.next_tag(&self->event_queue)) == 0);

    // TODO: The critical section could be smaller.
    if (env->has_physical_action) {
      env->platform->leave_critical_section(env->platform);
    }

    self->run_timestep(self);
    self->clean_up_timestep(self);

    if (env->has_physical_action) {
      env->platform->enter_critical_section(env->platform);
    }
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
  self->executing_tag = false;
  EventQueue_ctor(&self->event_queue);
  ReactionQueue_ctor(&self->reaction_queue);
}
