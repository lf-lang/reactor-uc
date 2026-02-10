#include "reactor-uc/action.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/trigger.h"

#include <string.h>

void Action_cleanup(Trigger* self) {
  LF_DEBUG(TRIG, "Cleaning up action %p", self);
  self->is_present = false;
}

void Action_prepare(Trigger* self, Event* event) {
  LF_DEBUG(TRIG, "Preparing action %p", self);
  Environment* env = self->parent->env;
  Action* act = (Action*)self;
  Scheduler* sched = env->scheduler;
  memcpy(act->value_ptr, event->super.payload, act->payload_pool.payload_size);

  if (self->is_present) {
    LF_WARN(TRIG, "Action %p is already present at this tag. Its value was overwritten", self);
  } else {
    sched->register_for_cleanup(sched, self);
    for (size_t i = 0; i < act->effects.size; i++) {
      validate(sched->add_to_reaction_queue(sched, act->effects.reactions[i]) == LF_OK);
    }
  }

  act->events_scheduled--;
  self->is_present = true;
  self->payload_pool->free(self->payload_pool, event->super.payload);
}

lf_ret_t Action_schedule(Action* self, interval_t offset, const void* value) {
  lf_ret_t ret;
  Environment* env = self->super.parent->env;
  Scheduler* sched = env->scheduler;
  EventQueue* event_queue = sched->get_event_queue(sched);
  void* payload = NULL;

  // this validates that no value is scheduled on a void action
  LF_INFO(TRIG, "%p %i %i", value, self->payload_pool.capacity, self->payload_pool.payload_size);
  validate(!(value == NULL && self->payload_pool.capacity > 0));

  if (self->events_scheduled >= self->max_pending_events) {
    LF_ERR(TRIG, "Action event buffer is full, dropping event. Capacity is %i", self->max_pending_events);
    return LF_VALUE_BUFFER_FULL;
  }

  if (value != NULL) {
    validate(self->payload_pool.capacity > 0);
    ret = self->payload_pool.allocate(&self->payload_pool, &payload);
    validate(ret == LF_OK);
    memcpy(payload, value, self->payload_pool.payload_size);
  }

  tag_t base_tag = ZERO_TAG;
  interval_t total_offset = lf_time_add(self->min_offset, offset);

  if (self->type == PHYSICAL_ACTION) {
    base_tag.time = env->get_physical_time(env);
  } else {
    base_tag = sched->current_tag(sched);
  }

  tag_t tag = lf_delay_tag(base_tag, total_offset);

  instant_t earliest_time = lf_time_add(self->last_event_time, self->min_spacing);

  if (earliest_time > tag.time || (earliest_time == tag.time && self->min_spacing == 0LL)) {
    LF_DEBUG(TRIG, "Event on action %p violates min_spacing. Policy is %d.", self, self->policy);

    Event target_evt = EVENT_INIT(tag, (Trigger*)self, NULL);
    Event* found = (Event*)event_queue->find_equal_same_tag(event_queue, &target_evt.super);

    switch (self->policy) {
    case drop:
      LF_DEBUG(TRIG, "Dropping event on action %p scheduled at time (%lld, %d) because it violates min_spacing.", self,
               tag.time, tag.microstep);
      return LF_OK;

    case update:
      if (found != NULL) {
        LF_DEBUG(TRIG, "Updating event on action %p. Removing old event scheduled at time (%lld, %d).", self,
                 found->super.tag.time, found->super.tag.microstep);
        lf_ret_t ret = event_queue->remove(event_queue, &found->super);
        event_queue->remove(event_queue, &found->super);
        validate(ret == LF_OK);
      }
      break;

    case replace:
      if (found != NULL) {
        found->super.payload = payload;
        LF_DEBUG(TRIG, "Replacing payload of event on action %p at time (%lld, %d).", self, tag.time, tag.microstep);
        return LF_OK;
      }
    /* fallthrough - no existing event, defer to earliest_time */
    case defer:
    default:
      tag.microstep = (earliest_time == tag.time && found != NULL) ? found->super.tag.microstep + 1 : 0;
      tag.time = earliest_time;
      LF_DEBUG(TRIG, "Deferring event on action %p to time (%lld, %d).", self, tag.time, tag.microstep);

      break;
    }
  }

  // If the event is scheduled at the current tag, we need to increment the microstep to ensure that the event is
  // scheduled after the currently executing reactions.
  if (lf_tag_compare(tag, env->scheduler->current_tag(env->scheduler)) <= 0) {
    tag.time = env->scheduler->current_tag(env->scheduler).time;
    tag.microstep = env->scheduler->current_tag(env->scheduler).microstep + 1;
  }

  Event event = EVENT_INIT(tag, (Trigger*)self, payload);

  ret = sched->schedule_at(sched, &event);

  if (ret == LF_OK) {
    self->events_scheduled++;
    self->last_event_time = tag.time;
  }

  return ret;
}

void Action_ctor(Action* self, ActionType type, ActionPolicy policy, interval_t min_offset, interval_t min_spacing,
                 Reactor* parent, Reaction** sources, size_t sources_size, Reaction** effects, size_t effects_size,
                 Reaction** observers, size_t observers_size, void* value_ptr, size_t value_size, void* payload_buf,
                 bool* payload_used_buf, size_t event_bound) {
  int capacity = 0;
  if (payload_buf != NULL) {
    capacity = event_bound;
  }
  EventPayloadPool_ctor(&self->payload_pool, (char*)payload_buf, payload_used_buf, value_size, capacity, 0);
  Trigger_ctor(&self->super, TRIG_ACTION, parent, &self->payload_pool, Action_prepare, Action_cleanup);

  self->type = type;
  self->policy = policy;
  self->value_ptr = value_ptr;
  self->min_offset = min_offset;
  self->min_spacing = min_spacing;
  self->last_event_time = NEVER;
  self->max_pending_events = event_bound;
  self->events_scheduled = 0;
  self->schedule = Action_schedule;
  self->sources.reactions = sources;
  self->sources.num_registered = 0;
  self->sources.size = sources_size;
  self->effects.reactions = effects;
  self->effects.size = effects_size;
  self->effects.num_registered = 0;
  self->observers.reactions = observers;
  self->observers.size = observers_size;
  self->observers.num_registered = 0;

  if (type == PHYSICAL_ACTION) {
    self->super.parent->env->has_async_events = true;
  }
}

void LogicalAction_ctor(LogicalAction* self, ActionPolicy policy, interval_t min_offset, interval_t min_spacing,
                        Reactor* parent, Reaction** sources, size_t sources_size, Reaction** effects,
                        size_t effects_size, Reaction** observers, size_t observers_size, void* value_ptr,
                        size_t value_size, void* payload_buf, bool* payload_used_buf, size_t event_bound) {
  Action_ctor(&self->super, LOGICAL_ACTION, policy, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, observers, observers_size, value_ptr, value_size, payload_buf, payload_used_buf,
              event_bound);
}

static lf_ret_t PhysicalAction_schedule(Action* super, interval_t offset, const void* value) {
  PhysicalAction* self = (PhysicalAction*)super;

  // We need a mutex because this can be called asynchronously and reads the events_scheduled variable.
  MUTEX_LOCK(self->mutex);
  lf_ret_t ret = Action_schedule(super, offset, value);
  MUTEX_UNLOCK(self->mutex);
  return ret;
}

static void PhysicalAction_prepare(Trigger* super, Event* event) {
  PhysicalAction* self = (PhysicalAction*)super;
  // We need a mutex because this writes the events_scheduled variable that is read from async context.
  MUTEX_LOCK(self->mutex);
  Action_prepare(super, event);
  MUTEX_UNLOCK(self->mutex);
}

void PhysicalAction_ctor(PhysicalAction* self, ActionPolicy policy, interval_t min_offset, interval_t min_spacing,
                         Reactor* parent, Reaction** sources, size_t sources_size, Reaction** effects,
                         size_t effects_size, Reaction** observers, size_t observers_size, void* value_ptr,
                         size_t value_size, void* payload_buf, bool* payload_used_buf, size_t event_bound) {
  Action_ctor(&self->super, PHYSICAL_ACTION, policy, min_offset, min_spacing, parent, sources, sources_size, effects,
              effects_size, observers, observers_size, value_ptr, value_size, payload_buf, payload_used_buf,
              event_bound);
  self->super.schedule = PhysicalAction_schedule;
  self->super.super.prepare = PhysicalAction_prepare;
  Mutex_ctor(&self->mutex.super);
}
