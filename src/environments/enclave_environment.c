#include "reactor-uc/environments/enclave_environment.h"
#include "reactor-uc/connection.h"
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

static void EnclaveEnvironment_start_at(Environment *super, instant_t start_time) {
  EnclaveEnvironment *self = (EnclaveEnvironment *)super;
  LF_INFO(ENV, "Starting enclave %s " PRINTF_TIME " nsec", super->main->name, start_time);

  self->super.scheduler->set_and_schedule_start_tag(self->super.scheduler, start_time);
  lf_ret_t ret = super->platform->create_thread(super->platform, &self->thread.super, enclave_thread, super);
  validate(ret == LF_OK);
}

void EnclaveEnvironment_join(Environment *super) {
  EnclaveEnvironment *self = (EnclaveEnvironment *)super;
  lf_ret_t ret = super->platform->join_thread(super->platform, &self->thread.super);
  validate(ret == LF_OK);
}

/**
 * @brief Acquire a tag by iterating through all network input ports and making
 * sure that they are resolved at this tag. If the input port is unresolved we
 * must wait for the max_wait time before proceeding.
 *
 * @param self
 * @param next_tag
 * @return lf_ret_t
 */
static lf_ret_t EnclaveEnvironment_acquire_tag(Environment *super, tag_t next_tag) {
  LF_DEBUG(SCHED, "Acquiring tag " PRINTF_TAG, next_tag);
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