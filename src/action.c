#include "reactor-uc/action.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/trigger.h"

#include <string.h>

void Action_cleanup(Trigger *self) {
  LF_DEBUG(TRIG, "Cleaning up action %p", self);
  self->is_present = false;
}

void Action_prepare(Trigger *self, Event *event) {
  LF_DEBUG(TRIG, "Preparing action %p", self);
  Action *act = (Action *)self;
  Scheduler *sched = &self->parent->env->scheduler;
  memcpy(act->value_ptr, event->payload, act->payload_pool.size);

  if (self->is_present) {
    LF_WARN(TRIG, "Action %p is already present at this tag. Its value was overwritten", self);
  } else {
    sched->register_for_cleanup(sched, self);
    for (size_t i = 0; i < act->effects.size; i++) {
      validate(sched->reaction_queue.insert(&sched->reaction_queue, act->effects.reactions[i]) == LF_OK);
    }
  }

  self->is_present = true;
  self->payload_pool->free(self->payload_pool, event->payload);
}

lf_ret_t Action_schedule(Action *self, interval_t offset, const void *value) {
  lf_ret_t ret;
  Environment *env = self->super.parent->env;
  Scheduler *sched = &env->scheduler;
  void *payload = NULL;
  validate(value);

  env->enter_critical_section(env);

  // Dont accept events before we have started
  // TODO: Do we instead need some flag to signal that we have started?
  if (sched->start_time == NEVER) {
    LF_ERR(TRIG, "Action %p cannot schedule events before start tag", self);
    env->leave_critical_section(env);
    return LF_ERR;
  }

  ret = self->super.payload_pool->allocate(self->super.payload_pool, &payload);
  if (ret != LF_OK) {
    return ret;
  }

  memcpy(payload, value, self->payload_pool.size);

  tag_t base_tag = ZERO_TAG;
  interval_t total_offset = lf_time_add(self->min_offset, offset);

  if (self->type == PHYSICAL_ACTION) {
    base_tag.time = env->get_physical_time(env);
  } else {
    base_tag = sched->current_tag;
  }

  tag_t tag = lf_delay_tag(base_tag, total_offset);
  Event event = EVENT_INIT(tag, (Trigger *)self, payload);

  ret = sched->schedule_at_locked(sched, &event);

  if (self->type == PHYSICAL_ACTION) {
    env->platform->new_async_event(env->platform);
  }

  env->leave_critical_section(env);

  return ret;
}

void Action_ctor(Action *self, ActionType type, interval_t min_offset, Reactor *parent, Reaction **sources,
                 size_t sources_size, Reaction **effects, size_t effects_size, void *value_ptr, size_t value_size,
                 void *payload_buf, bool *payload_used_buf, size_t payload_buf_capacity) {
  EventPayloadPool_ctor(&self->payload_pool, payload_buf, payload_used_buf, value_size, payload_buf_capacity);
  Trigger_ctor(&self->super, TRIG_ACTION, parent, &self->payload_pool, Action_prepare, Action_cleanup);
  self->type = type;
  self->value_ptr = value_ptr;
  self->min_offset = min_offset;
  self->schedule = Action_schedule;
  self->sources.reactions = sources;
  self->sources.num_registered = 0;
  self->sources.size = sources_size;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;

  if (type == PHYSICAL_ACTION) {
    self->super.parent->env->has_async_events = true;
  }
}
