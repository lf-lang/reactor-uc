#include "reactor-uc/environments/enclave_environment.h"
#include "reactor-uc/enclaved.h"
#include "reactor-uc/port.h"
#include "reactor-uc/logging.h"

static void EnclaveEnvironment_shutdown(Environment *env);

static void *enclave_thread(void *environment_pointer) {
  Environment *env = (Environment *)environment_pointer;
  env->scheduler->run(env->scheduler);

  // Unblock any downstream enclaves
  EnclaveEnvironment_shutdown(env);

  return NULL;
}

static void EnclaveEnvironment_shutdown(Environment *super) {
  Reactor *main = super->main;
  for (size_t i = 0; i < main->triggers_size; i++) {
    Trigger *trigger = main->triggers[i];
    if (trigger->type == TRIG_OUTPUT) {
      Port *output = (Port *)trigger;
      for (size_t j = 0; j < output->conns_out_registered; j++) {
        Connection *conn = output->conns_out[j];
        if (conn->super.type == TRIG_CONN_ENCLAVED) {
          EnclavedConnection *enclaved_conn = (EnclavedConnection *)conn;
          enclaved_conn->set_last_known_tag(enclaved_conn, FOREVER_TAG);
        }
      }
    }
  }
}

/**
 * @brief Search recursively down the hierarchy for more enclaves and start them.
 *
 * @param current_env The environment of the current enclave.
 * @param reactor The reactor to inspect and possibly search further down from.
 * @param start_time The start time of the program.
 */
static void EnclaveEnvironment_start_at_nested(Environment *current_env, Reactor *reactor, instant_t start_time) {
  if (reactor->env != current_env && reactor->env->type == ENVIRONMENT_ENCLAVE) {
    reactor->env->start_at(reactor->env, start_time);
  } else {
    for (size_t i = 0; i < reactor->children_size; i++) {
      EnclaveEnvironment_start_at_nested(current_env, reactor->children[i], start_time);
    }
  }
}

static void EnclaveEnvironment_start_at(Environment *super, instant_t start_time) {
  EnclaveEnvironment *self = (EnclaveEnvironment *)super;

  // Before starting this enclave, we search down and see if there are contained enclaves
  // that we start first.
  for (size_t i = 0; i < super->main->children_size; i++) {
    EnclaveEnvironment_start_at_nested(super, super->main->children[i], start_time);
  }

  LF_INFO(ENV, "Starting enclave %s at " PRINTF_TIME " nsec", super->main->name, start_time);

  self->super.scheduler->set_and_schedule_start_tag(self->super.scheduler, start_time);
  lf_ret_t ret = super->platform->create_thread(super->platform, &self->thread.super, enclave_thread, super);
  validate(ret == LF_OK);
}

/**
 * @brief Recursively find nested enclaves and join on them
 */
static void EnclaveEnvironment_join_nested(Environment *current_env, Reactor *reactor) {
  if (reactor->env != current_env && reactor->env->type == ENVIRONMENT_ENCLAVE) {
    reactor->env->join(reactor->env);
  } else {
    for (size_t i = 0; i < reactor->children_size; i++) {
      EnclaveEnvironment_join_nested(current_env, reactor->children[i]);
    }
  }
}

static void EnclaveEnvironment_join(Environment *super) {
  EnclaveEnvironment *self = (EnclaveEnvironment *)super;
  // Before joining on this thread, check for any contained enclave and join them first.
  for (size_t i = 0; i < super->main->children_size; i++) {
    EnclaveEnvironment_join_nested(super, super->main->children[i]);
  }
  lf_ret_t ret = super->platform->join_thread(super->platform, &self->thread.super);
  validate(ret == LF_OK);
}

/**
 * @brief Acquire a tag for an enclave by looking at all the input port of the top-level reactor
 * in the enclave. If they are connected to an EnclavedConnection, then we check the last known
 * tag of that connection and the maxwait of the input port.
 *
 * @param self
 * @param next_tag
 * @return lf_ret_t LF_OK if tag is acquired, LF_SLEEP_INTERRUPTED if we were interrupted before acquiring the tag.
 */
static lf_ret_t EnclaveEnvironment_acquire_tag(Environment *super, tag_t next_tag) {
  LF_DEBUG(SCHED, "Enclave %s acquiring tag " PRINTF_TAG, super->main->name, next_tag);
  Reactor *enclave = super->main;
  instant_t additional_sleep = 0;
  for (size_t i = 0; i < enclave->triggers_size; i++) {
    Trigger *trigger = enclave->triggers[i];

    if (trigger->type == TRIG_INPUT) {
      Port *input = (Port *)trigger;
      if (!input->conn_in)
        continue;
      if (input->conn_in->super.type == TRIG_CONN_ENCLAVED) {
        EnclavedConnection *conn = (EnclavedConnection *)input->conn_in;

        if (conn->type == PHYSICAL_CONNECTION)
          continue;

        tag_t last_known_tag = conn->get_last_known_tag(conn);
        if (lf_tag_compare(last_known_tag, next_tag) < 0) {
          LF_DEBUG(SCHED, "Input %p is unresolved, latest known tag was " PRINTF_TAG, input, last_known_tag);
          LF_DEBUG(SCHED, "Input %p has maxwait of  " PRINTF_TIME, input, input->max_wait);
          if (input->max_wait > additional_sleep) {
            additional_sleep = input->max_wait;
          }
        }
      }
    }
  }

  if (additional_sleep > 0) {
    LF_DEBUG(SCHED, "Need to sleep for additional " PRINTF_TIME " ns", additional_sleep);
    instant_t sleep_until = lf_time_add(next_tag.time, additional_sleep);
    return super->wait_until(super, sleep_until);
  } else {
    return LF_OK;
  }
}

void EnclaveEnvironment_ctor(EnclaveEnvironment *self, Reactor *main, Scheduler *scheduler, bool fast_mode) {
  Environment_ctor(&self->super, ENVIRONMENT_ENCLAVE, main, scheduler, fast_mode);
  self->super.start_at = EnclaveEnvironment_start_at;
  self->super.join = EnclaveEnvironment_join;
  self->super.has_async_events = true;
  self->super.acquire_tag = EnclaveEnvironment_acquire_tag;
}