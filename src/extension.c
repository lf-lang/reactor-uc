#include "reactor-uc/extension.h"
#include "reactor-uc/action.h"
#include "reactor-uc/connection.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/timer.h"

void LfGate_ctor(LfGate* self) { *self = (LfGate){0}; }
void LfGateCondition_ctor(LfGateCondition* self) { *self = (LfGateCondition){0}; }

lf_ret_t LfGate_add(LfGate* self, LfGateCondition* condition, const bool* value) {
  if (self == NULL || condition == NULL || value == NULL) {
    return LF_INVALID_VALUE;
  }
  if (condition->_owner != NULL) {
    return condition->_owner == self && condition->value == value ? LF_OK : LF_INVALID_VALUE;
  }
  condition->value = value;
  condition->_owner = self;
  condition->_next = self->_head;
  self->_head = condition;
  return LF_OK;
}

bool LfGate_contains(const LfGate* self, const bool* value) {
  if (self == NULL) {
    return false;
  }
  for (const LfGateCondition* condition = self->_head; condition != NULL; condition = condition->_next) {
    if (condition->value == value) {
      return true;
    }
  }
  return false;
}

#if LF_RUNTIME_EXTENSIONS
void LfExtensionState_ctor(LfExtensionState* self) { *self = (LfExtensionState){.last_completed_tag = NEVER_TAG}; }

lf_ret_t Environment_bind_extensions(Environment* self, void* context) {
  if (self == NULL || self->_extensions.sealed || (self->_extensions.bound && self->_extensions.context != context)) {
    return LF_INVALID_VALUE;
  }
  self->_extensions.context = context;
  self->_extensions.bound = true;
  return LF_OK;
}

static bool LfExtension_in_setup(const Reactor* parent) {
  if (parent == NULL || parent->env == NULL || parent->env->_extensions.sealed) {
    return false;
  }
  return true;
}

lf_ret_t LfReaction_add_gate(Reaction* reaction, LfGateCondition* condition, const bool* value) {
  if (reaction == NULL || !LfExtension_in_setup(reaction->parent)) {
    return LF_INVALID_VALUE;
  }
  return LfGate_add(&reaction->gate, condition, value);
}

lf_ret_t LfTimer_add_gate(Timer* timer, LfGateCondition* condition, const bool* value) {
  if (timer == NULL || !LfExtension_in_setup(timer->super.parent)) {
    return LF_INVALID_VALUE;
  }
  return LfGate_add(&timer->gate, condition, value);
}

lf_ret_t LfAction_add_admission_gate(Action* action, LfGateCondition* condition, const bool* value) {
  if (action == NULL || !LfExtension_in_setup(action->super.parent)) {
    return LF_INVALID_VALUE;
  }
  return LfGate_add(&action->schedule_gate, condition, value);
}

/* Only triggers whose events wait in the event queue can be controlled. */
static bool LfEventControl_controllable(const Trigger* trigger) {
  if (trigger == NULL) {
    return false;
  }
  switch (trigger->type) {
  case TRIG_ACTION:
  case TRIG_TIMER:
  case TRIG_CONN_DELAYED:
    return true;
  default:
    return false;
  }
}

size_t LfEventControl_bound(const Trigger* trigger) {
  if (trigger == NULL) {
    return 0;
  }
  switch (trigger->type) {
  case TRIG_ACTION:
    return ((const Action*)trigger)->max_pending_events;
  case TRIG_TIMER:
    return 1;
  case TRIG_CONN_DELAYED:
    return ((const DelayedConnection*)trigger)->max_pending_events;
  default:
    return 0;
  }
}

/* An action's budget is shared with Action_schedule, which may run asynchronously for
 * physical actions under the action's own mutex. */
static void LfEventControl_lock(Trigger* trigger) {
  if (trigger->type == TRIG_ACTION && ((Action*)trigger)->type == PHYSICAL_ACTION) {
    MUTEX_LOCK(((PhysicalAction*)trigger)->mutex);
  }
}

static void LfEventControl_unlock(Trigger* trigger) {
  if (trigger->type == TRIG_ACTION && ((Action*)trigger)->type == PHYSICAL_ACTION) {
    MUTEX_UNLOCK(((PhysicalAction*)trigger)->mutex);
  }
}

/* A retained event still counts against its action's bound until it is delivered or
 * released here. */
static void LfEventControl_release(Trigger* trigger, void* payload) {
  if (payload != NULL && trigger->payload_pool != NULL) {
    validate(trigger->payload_pool->free(trigger->payload_pool, payload) == LF_OK);
  }
  if (trigger->type == TRIG_ACTION) {
    Action* action = (Action*)trigger;
    LfEventControl_lock(trigger);
    validate(action->events_scheduled > 0);
    action->events_scheduled--;
    LfEventControl_unlock(trigger);
  } else if (trigger->type == TRIG_CONN_DELAYED) {
    DelayedConnection* connection = (DelayedConnection*)trigger;
    validate(connection->events_scheduled > 0);
    connection->events_scheduled--;
  }
}

lf_ret_t LfEventControl_take(Trigger* trigger, Event* out, size_t capacity, size_t* count) {
  if (count == NULL) {
    return LF_INVALID_VALUE;
  }
  *count = 0;
  if (!LfEventControl_controllable(trigger) || (out == NULL && capacity > 0)) {
    return LF_INVALID_VALUE;
  }
  Scheduler* sched = trigger->parent->env->scheduler;
  return sched->take_events_by_trigger(sched, trigger, out, capacity, count);
}

lf_ret_t LfEventControl_restore(Trigger* trigger, const Event* events, size_t count, interval_t shift) {
  if (!LfEventControl_controllable(trigger) || (events == NULL && count > 0)) {
    return LF_INVALID_VALUE;
  }
  Scheduler* sched = trigger->parent->env->scheduler;
  const tag_t now = sched->current_tag(sched);
  instant_t latest = NEVER;

  for (size_t i = 0; i < count; i++) {
    Event event = events[i];
    validate(event.trigger == trigger);
    event.super.tag.time = lf_time_add(event.super.tag.time, shift);
    // A retained event can no longer be delivered in the past or at the current tag.
    if (lf_tag_compare(event.super.tag, now) <= 0) {
      event.super.tag = lf_delay_tag(now, 0);
    }
    event.intended_tag = event.super.tag;

    lf_ret_t ret = sched->schedule_at(sched, &event);
    if (ret != LF_OK) {
      LF_DEBUG(TRIG, "Restoring an event of trigger %p returned %d; releasing it", (void*)trigger, ret);
      LfEventControl_release(trigger, event.super.payload);
      continue;
    }
    if (event.super.tag.time > latest) {
      latest = event.super.tag.time;
    }
  }

  if (trigger->type == TRIG_ACTION && latest != NEVER) {
    Action* action = (Action*)trigger;
    LfEventControl_lock(trigger);
    if (latest > action->last_event_time) {
      action->last_event_time = latest;
    }
    LfEventControl_unlock(trigger);
  }
  return LF_OK;
}

lf_ret_t LfEventControl_discard(Trigger* trigger, const Event* events, size_t count) {
  if (!LfEventControl_controllable(trigger) || (events == NULL && count > 0)) {
    return LF_INVALID_VALUE;
  }
  for (size_t i = 0; i < count; i++) {
    validate(events[i].trigger == trigger);
    LfEventControl_release(trigger, events[i].super.payload);
  }
  return LF_OK;
}

lf_ret_t LfEventControl_purge(Trigger* trigger, size_t* count) {
  if (count == NULL) {
    return LF_INVALID_VALUE;
  }
  *count = 0;
  if (!LfEventControl_controllable(trigger)) {
    return LF_INVALID_VALUE;
  }
  Scheduler* sched = trigger->parent->env->scheduler;
  // First lock the trigger, then the queue (like Action_schedule) 
  LfEventControl_lock(trigger);
  lf_ret_t ret = sched->purge_events_by_trigger(sched, trigger, count);
  if (ret == LF_OK && trigger->type == TRIG_ACTION) {
    Action* action = (Action*)trigger;
    validate(action->events_scheduled >= *count);
    action->events_scheduled -= *count;
  } else if (ret == LF_OK && trigger->type == TRIG_CONN_DELAYED) {
    DelayedConnection* connection = (DelayedConnection*)trigger;
    validate(connection->events_scheduled >= *count);
    connection->events_scheduled -= *count;
  }
  LfEventControl_unlock(trigger);
  return ret;
}

lf_ret_t LfTimerControl_suspend(Timer* timer, interval_t* remaining) {
  if (remaining == NULL) {
    return LF_INVALID_VALUE;
  }
  *remaining = NEVER;
  if (timer == NULL) {
    return LF_INVALID_VALUE;
  }
  Scheduler* sched = timer->super.parent->env->scheduler;
  Event taken;
  size_t count = 0;
  lf_ret_t ret = sched->take_events_by_trigger(sched, &timer->super, &taken, 1, &count);
  if (ret != LF_OK) {
    return ret;
  }
  if (count == 0) {
    return LF_EVENT_NOT_FOUND;
  }
  // Timer events carry no payload and no budget, so nothing else needs releasing.
  *remaining = taken.super.tag.time - sched->current_tag(sched).time;
  return LF_OK;
}

lf_ret_t LfTimerControl_arm(Timer* timer, interval_t delay) {
  if (timer == NULL) {
    return LF_INVALID_VALUE;
  }
  Scheduler* sched = timer->super.parent->env->scheduler;
  size_t purged = 0;
  lf_ret_t ret = sched->purge_events_by_trigger(sched, &timer->super, &purged);
  if (ret != LF_OK) {
    return ret;
  }
  tag_t tag = lf_delay_tag(sched->current_tag(sched), delay < 0 ? 0 : delay);
  Event event = EVENT_INIT(tag, &timer->super, NULL);
  return sched->schedule_at(sched, &event);
}

lf_ret_t LfExtension_enqueue_reaction(Environment* env, Reaction* reaction) {
  if (env == NULL || reaction == NULL) {
    return LF_INVALID_VALUE;
  }
  return env->scheduler->add_to_reaction_queue(env->scheduler, reaction);
}

#endif
