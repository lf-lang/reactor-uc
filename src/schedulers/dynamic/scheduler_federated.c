
#include "reactor-uc/schedulers/dynamic/scheduler_federated.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/environments/environment_federated.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/tag.h"

/**
 * @brief Acquire a tag by iterating through all network input ports and making
 * sure that they are resolved at this tag. If the input port is unresolved we
 * must wait for the max_wait time before proceeding.
 *
 * @param self
 * @param next_tag
 * @return lf_ret_t
 */
static lf_ret_t DynamicSchedulerFederated_acquire_tag(DynamicScheduler *self, tag_t next_tag) {
  EnvironmentFederated *env_fed = (EnvironmentFederated *)self->env;

  LF_DEBUG(SCHED, "Acquiring tag " PRINTF_TAG, next_tag);
  Environment *env = self->env;
  instant_t additional_sleep = 0;
  for (size_t i = 0; i < env_fed->net_bundles_size; i++) {
    FederatedConnectionBundle *bundle = env_fed->net_bundles[i];

    if (!bundle->net_channel->is_connected(bundle->net_channel)) {
      continue;
    }

    for (size_t j = 0; j < bundle->inputs_size; j++) {
      FederatedInputConnection *input = bundle->inputs[j];
      // Find the max safe-to-assume-absent value and go to sleep waiting for this.
      if (lf_tag_compare(input->last_known_tag, next_tag) < 0) {
        LF_DEBUG(SCHED, "Input %p is unresolved, latest known tag was " PRINTF_TAG, input, input->last_known_tag);
        LF_DEBUG(SCHED, "Input %p has maxwait of  " PRINTF_TIME, input, input->max_wait);
        if (input->max_wait > additional_sleep) {
          additional_sleep = input->max_wait;
        }
      }
    }
  }

  if (additional_sleep > 0) {
    LF_DEBUG(SCHED, "Need to sleep for additional " PRINTF_TIME " ns", additional_sleep);
    instant_t sleep_until = lf_time_add(next_tag.time, additional_sleep);
    return env->wait_until_locked(env, sleep_until);
  } else {
    return LF_OK;
  }
}

void DynamicSchedulerFederated_ctor(DynamicSchedulerFederated *self, EnvironmentFederated *env, EventQueue *event_queue,
                                    EventQueue *system_event_queue, ReactionQueue *reaction_queue, interval_t duration,
                                    bool keep_alive) {
  DynamicScheduler_ctor(&self->super, &env->super, event_queue, system_event_queue, reaction_queue, duration,
                        keep_alive);
  self->super.acquire_tag = DynamicSchedulerFederated_acquire_tag;
}
