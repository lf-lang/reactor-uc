#include "reactor-uc/action.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/trigger.h"

#include <assert.h>
#include <string.h>

void Action_cleanup(Trigger *self) {
  LF_DEBUG(TRIG, "Cleaning up action %p", self);
  self->is_present = false;
}

void Action_prepare(Trigger *self, Event *event) {
  LF_DEBUG(TRIG, "Preparing action %p", self);
  Action *act = (Action *)self;
  Scheduler *sched = &self->parent->env->scheduler;
  self->is_present = true;
  memcpy(act->value_ptr, event->payload, act->payload_pool.size);

  sched->register_for_cleanup(sched, self);

  for (size_t i = 0; i < act->effects.size; i++) {
    validaten(sched->reaction_queue.insert(&sched->reaction_queue, act->effects.reactions[i]));
  }
  self->payload_pool->free(self->payload_pool, event->payload);
}

lf_ret_t Action_schedule(Action *self, interval_t offset, const void *value) {
  lf_ret_t ret;
  Environment *env = self->super.parent->env;
  Scheduler *sched = &env->scheduler;
  void *payload = NULL;
  validate(value);

  env->enter_critical_section(env);

  ret = self->super.payload_pool->allocate(self->super.payload_pool, &payload);
  if (ret != LF_OK) {
    return ret;
  }

  memcpy(payload, value, self->payload_pool.size);

  tag_t base_tag = ZERO_TAG;
  interval_t total_offset = lf_time_add(self->min_offset, offset);

  if (self->is_physical) {
    base_tag.time = env->get_physical_time(env);
  } else {
    base_tag = sched->current_tag;
  }

  tag_t proposed_tag = lf_delay_tag(base_tag, total_offset);
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(proposed_tag, earliest_allowed) >= 0) {
    Event event = EVENT_INIT(proposed_tag, (Trigger *)self, payload);

    ret = sched->schedule_at_locked(sched, &event);

    if (ret == LF_OK) {
      self->previous_event = proposed_tag;
    }
  }
  if (self->is_physical) {
    env->platform->new_async_event(env->platform);
  }
  env->leave_critical_section(env);

  return ret;
}

void Action_ctor(Action *self, interval_t min_offset, interval_t min_spacing, bool is_physical, Reactor *parent,
                 Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size, void *value_ptr,
                 size_t value_size, void *payload_buf, bool *payload_used_buf, size_t payload_buf_capacity) {
  EventPayloadPool_ctor(&self->payload_pool, payload_buf, payload_used_buf, value_size, payload_buf_capacity);
  Trigger_ctor(&self->super, TRIG_ACTION, parent, &self->payload_pool, Action_prepare, Action_cleanup);
  self->is_physical = is_physical;
  self->value_ptr = value_ptr;
  self->min_offset = min_offset;
  self->min_spacing = min_spacing;
  self->previous_event = NEVER_TAG;
  self->schedule = Action_schedule;
  self->sources.reactions = sources;
  self->sources.num_registered = 0;
  self->sources.size = sources_size;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;

  if (is_physical) {
    self->super.parent->env->has_async_events = true;
  }
}
