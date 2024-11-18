//
// Created by tanneberger on 11/16/24.
//

#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/schedulers/static/scheduler.h"

Reaction* lf_sched_get_ready_reaction(StaticScheduler* scheduler, int worker_number) {
  LF_PRINT_DEBUG("Worker %d inside lf_sched_get_ready_reaction", worker_number);

  const inst_t*   current_schedule    = scheduler->static_schedule[worker_number];
  Reaction*       returned_reaction   = NULL;
  bool            exit_loop           = false;
  size_t*         pc                  = &scheduler->pc[worker_number];

  function_virtual_instruction_t func;
  operand_t       op1;
  operand_t       op2;
  operand_t       op3;
  bool            debug;

  while (!exit_loop) {
    func = current_schedule[*pc].func;
    op1 = current_schedule[*pc].op1;
    op2 = current_schedule[*pc].op2;
    op3 = current_schedule[*pc].op3;
    debug = current_schedule[*pc].debug;

    // Execute the current instruction
    func(scheduler->env->platform, worker_number, op1, op2, op3, debug, pc,
                &returned_reaction, &exit_loop);
  }

  LF_PRINT_DEBUG("Worker %d leaves lf_sched_get_ready_reaction", worker_number);
  return returned_reaction;
}


void StaticScheduler_ctor(StaticScheduler* self, Environment *env) {
  self->env = env;

  /*
  self->keep_alive = false;
  self->stop_tag = FOREVER_TAG;
  self->current_tag = NEVER_TAG;
  self->start_time = NEVER;
  self->duration = FOREVER;
  self->cleanup_ll_head = NULL;
  self->cleanup_ll_tail = NULL;
  self->leader = false;
  */

  self->scheduler->run = Scheduler_run;
  self->scheduler->prepare_timestep = Scheduler_prepare_timestep;
  self->scheduler->clean_up_timestep = Scheduler_clean_up_timestep;
  self->scheduler->run_timestep = Scheduler_run_timestep;
  self->scheduler->do_shutdown = Scheduler_do_shutdown;
  self->scheduler->schedule_at = Scheduler_schedule_at;
  self->scheduler->schedule_at_locked = Scheduler_schedule_at_locked;
  self->scheduler->register_for_cleanup = Scheduler_register_for_cleanup;
  self->scheduler->request_shutdown = Scheduler_request_shutdown;
  self->scheduler->acquire_and_schedule_start_tag = Scheduler_acquire_and_schedule_start_tag;

  Scheduler_ctor(self->scheduler);
}
