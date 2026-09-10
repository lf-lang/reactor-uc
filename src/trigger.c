#include "reactor-uc/trigger.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/action.h"
#include "reactor-uc/connection.h"
#include "reactor-uc/logging.h"

void Trigger_ctor(Trigger* self, TriggerType type, Reactor* parent, EventPayloadPool* payload_pool,
                  void (*prepare)(Trigger*, Event*), void (*cleanup)(Trigger*)) {
  self->type = type;
  self->parent = parent;
  self->next = NULL;
  self->is_present = false;
  self->is_registered_for_cleanup = false;
  self->prepare = prepare;
  self->cleanup = cleanup;
  self->payload_pool = payload_pool;
#if defined(LF_RUNTIME_EXTENSIONS)
  self->_extension_bindings = NULL;
#endif
}

#if defined(LF_RUNTIME_EXTENSIONS)

lf_ret_t Trigger_bind_extension(Trigger* self, LfExtensionBinding* binding, const LfExtensionDescriptor* descriptor,
                                void* state) {
  if (self == NULL || binding == NULL || descriptor == NULL ||
      descriptor->api_version != LF_RUNTIME_EXTENSION_API_VERSION ||
      descriptor->struct_size < sizeof(LfExtensionDescriptor)) {
    return LF_INVALID_VALUE;
  }
  if (binding->_owner != NULL && binding->_owner != self) {
    return LF_INVALID_VALUE;
  }
  for (LfExtensionBinding* it = self->_extension_bindings; it != NULL; it = it->_next) {
    if (it == binding) {
      return it->descriptor == descriptor && it->state == state ? LF_OK : LF_INVALID_VALUE;
    }
    if (it->descriptor == descriptor) {
      return LF_INVALID_VALUE;
    }
  }
  if (binding->_owner != NULL) {
    return LF_INVALID_VALUE;
  }

  binding->descriptor = descriptor;
  binding->state = state;
  binding->_owner = self;
  binding->_next = NULL;
  LfExtensionBinding** slot = &self->_extension_bindings;
  while (*slot != NULL) {
    slot = &(*slot)->_next;
  }
  *slot = binding;
  return LF_OK;
}

void* Trigger_extension_state(const Trigger* self, const LfExtensionDescriptor* descriptor) {
  if (self == NULL || descriptor == NULL) {
    return NULL;
  }
  for (const LfExtensionBinding* binding = self->_extension_bindings; binding != NULL; binding = binding->_next) {
    if (binding->descriptor == descriptor) {
      return binding->state;
    }
  }
  return NULL;
}

/**
 * @brief Credit a released event back to the budget of whichever trigger bounds it.
 *
 * Actions and delayed connections each cap their in-flight events with their own counter, so an
 * event that will never be delivered must be credited back to whichever charged for it. The
 * counters are `size_t`, so a double release wraps to SIZE_MAX and kills the trigger silently.
 */
static void Trigger_release_event_budget(Trigger* self) {
  if (self->type == TRIG_ACTION) {
    validate(((Action*)self)->events_scheduled > 0);
    ((Action*)self)->events_scheduled--;
  } else if (self->type == TRIG_CONN_DELAYED) {
    validate(((DelayedConnection*)self)->events_scheduled > 0);
    ((DelayedConnection*)self)->events_scheduled--;
  }
}

lf_ret_t Trigger_take_pending(Trigger* self, Event* out, size_t cap, size_t* n_out) {
  validate(self);
  validate(out);
  validate(n_out);
  Scheduler* sched = self->parent->env->scheduler;
  validate(sched->take_events_by_trigger != NULL);
  return sched->take_events_by_trigger(sched, self, out, cap, n_out);
}

lf_ret_t Trigger_restore_pending(Trigger* self, const Event* in_events, size_t n, interval_t shift) {
  validate(self);
  Scheduler* sched = self->parent->env->scheduler;
  instant_t latest = NEVER;

  for (size_t i = 0; i < n; i++) {
    Event event = in_events[i];
    event.super.tag.time = lf_time_add(event.super.tag.time, shift);
    event.intended_tag = event.super.tag;

    // Clamp into the future. A saved event can carry a non-zero microstep (a zero-delay
    // mode-local schedule lands at (T,1)).
    // If the re-entry commit happens at a microstep >= that one, the shifted tag
    // would be <= current_tag and schedule_at would return LF_PAST_TAG, silently
    // losing the event.
    if (lf_tag_compare(event.super.tag, sched->current_tag(sched)) <= 0) {
      event.super.tag = lf_delay_tag(sched->current_tag(sched), 0);
      event.intended_tag = event.super.tag;
    }

    lf_ret_t ret = sched->schedule_at(sched, &event);
    if (ret != LF_OK) {
      // Past the stop tag is expected during shutdown
      LF_DEBUG(TRIG, "Restoring event on trigger %p returned %d; dropping it", (void*)self, ret);
      if (self->payload_pool && event.super.payload) {
        self->payload_pool->free(self->payload_pool, event.super.payload);
      }
      Trigger_release_event_budget(self);
      continue;
    }
    if (event.super.tag.time > latest) {
      latest = event.super.tag.time;
    }
  }

  if (self->type == TRIG_ACTION && latest != NEVER && latest > ((Action*)self)->last_event_time) {
    ((Action*)self)->last_event_time = latest;
  }
  return LF_OK;
}

lf_ret_t Trigger_discard_pending(Trigger* self, const Event* in_events, size_t n) {
  validate(self);
  for (size_t i = 0; i < n; i++) {
    if (self->payload_pool && in_events[i].super.payload) {
      self->payload_pool->free(self->payload_pool, in_events[i].super.payload);
    }
    // The trigger's own prepare would have decremented this. The event will never be prepared.
    Trigger_release_event_budget(self);
  }
  return LF_OK;
}

lf_ret_t Trigger_purge_pending(Trigger* self, size_t* n_out) {
  validate(self);
  validate(n_out);
  Scheduler* sched = self->parent->env->scheduler;
  // A scheduler without the extension slots cannot support structural suspension at all.
  validate(sched->purge_events_by_trigger != NULL);

  size_t purged = 0;
  lf_ret_t ret = sched->purge_events_by_trigger(sched, self, &purged);
  if (ret != LF_OK) {
    *n_out = 0;
    return ret;
  }
  for (size_t i = 0; i < purged; i++) {
    Trigger_release_event_budget(self);
  }
  *n_out = purged;
  return LF_OK;
}

#endif /* LF_RUNTIME_EXTENSIONS */
