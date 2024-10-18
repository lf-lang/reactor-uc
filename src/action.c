#include "reactor-uc/action.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/trigger.h"

#include <assert.h>
#include <string.h>

// FIXME: Use default Trigger_cleanup?
void Action_cleanup(Trigger *self) {
  LF_DEBUG(TRIG, "Cleaning up action %p", self);
  self->is_present = false;
}

void Action_prepare(Trigger *self, Event *event) {
  LF_DEBUG(TRIG, "Preparing action %p", self);
  Action *act = (Action *)self;
  Scheduler *sched = &self->parent->env->scheduler;
  self->is_present = true;
  memcpy(self->value_ptr, event->payload, self->value_size);

  sched->register_for_cleanup(sched, self);

  for (size_t i = 0; i < act->effects.size; i++) {
    validaten(sched->reaction_queue.insert(&sched->reaction_queue, act->effects.reactions[i]));
  }
  self->payload_pool->free(self->payload_pool, event->payload);
}

void Action_ctor(Action *self, TriggerType type, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                 Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size, void *payload_buf,
                 bool *payload_used_buf, size_t payload_size, size_t payload_buf_capacity,
                 lf_ret_t (*schedule)(Action *, interval_t, const void *)) {
  EventPayloadPool_ctor(&self->payload_pool, payload_buf, payload_used_buf, payload_size, payload_buf_capacity);
  Trigger_ctor(&self->super, type, parent, &self->payload_pool, Action_prepare,
               Action_cleanup); // FIXME: Default cleanup function...
  self->min_offset = min_offset;
  self->min_spacing = min_spacing;
  self->previous_event = NEVER_TAG;
  self->schedule = schedule;
  self->sources.reactions = sources;
  self->sources.num_registered = 0;
  self->sources.size = sources_size;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;
}

lf_ret_t LogicalAction_schedule(Action *self, interval_t offset, const void *value) {
  lf_ret_t ret;
  Environment *env = self->super.parent->env;
  Scheduler *sched = &env->scheduler;
  void *payload = NULL;
  validate(value);

  ret = self->super.payload_pool->allocate(self->super.payload_pool, &payload);
  if (ret != LF_OK) {
    return ret;
  }

  memcpy(payload, value, self->super.value_size);

  tag_t proposed_tag = lf_delay_tag(sched->current_tag, offset);
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(proposed_tag, earliest_allowed) < 0) {
    return LF_INVALID_TAG;
  }
  Event event = {.tag = proposed_tag, .trigger = (Trigger *)self, .payload = payload};

  ret = sched->schedule_at(sched, &event);
  if (ret == LF_OK) {
    self->previous_event = proposed_tag;
  }
  return ret;
}

void LogicalAction_ctor(LogicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                        Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                        void *payload_buf, bool *payload_used_buf, size_t payload_size, size_t payload_buf_capacity) {
  Action_ctor(&self->super, TRIG_LOGICAL_ACTION, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, payload_buf, payload_used_buf, payload_size, payload_buf_capacity, LogicalAction_schedule);
}

// FIXME: Use new allocator here.
lf_ret_t PhysicalAction_schedule(Action *self, interval_t offset, const void *value) {
  lf_ret_t ret;
  Environment *env = self->super.parent->env;
  Scheduler *sched = &env->scheduler;

  env->platform->enter_critical_section(env->platform);

  tag_t tag = {.time = env->get_physical_time(env) + self->min_offset + offset, .microstep = 0};
  tag_t earliest_allowed = lf_delay_tag(self->previous_event, self->min_spacing);
  if (lf_tag_compare(tag, earliest_allowed) < 0) {
    env->platform->leave_critical_section(env->platform);
    return LF_INVALID_TAG;
  }
  void *payload = NULL;

  if (value) {
    EventPayloadPool *pool = self->super.payload_pool;
    ret = pool->allocate(pool, &payload);
    if (ret != LF_OK) {
      LF_ERR(TRIG, "Could not allocate new payload. Dropping event");
      env->leave_critical_section(env);
      return LF_OUT_OF_BOUNDS;
    }
  }
  Event event = {.tag = tag, .trigger = &self->super, .payload = payload};

  ret = sched->schedule_at_locked(sched, &event);
  if (ret == LF_OK) {
    self->previous_event = tag;
  }

  env->platform->new_async_event(env->platform);
  env->platform->leave_critical_section(env->platform);

  return LF_OK;
}

void PhysicalAction_ctor(PhysicalAction *self, interval_t min_offset, interval_t min_spacing, Reactor *parent,
                         Reaction **sources, size_t sources_size, Reaction **effects, size_t effects_size,
                         void *payload_buf, bool *payload_used_buf, size_t payload_size, size_t payload_buf_capacity) {
  Action_ctor(&self->super, TRIG_PHYSICAL_ACTION, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, payload_buf, payload_used_buf, payload_size, payload_buf_capacity, PhysicalAction_schedule);
  parent->env->has_async_events = true;
}
