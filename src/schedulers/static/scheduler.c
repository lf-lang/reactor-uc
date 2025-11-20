//
// Created by tanneberger on 11/16/24.
//

#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/schedulers/static/scheduler.h"

Reaction* lf_sched_get_ready_reaction(StaticScheduler* scheduler, int worker_number) {
  LF_PRINT_DEBUG("Worker %d inside lf_sched_get_ready_reaction", worker_number);

  const inst_t* current_schedule = scheduler->static_schedule[worker_number];
  Reaction* returned_reaction = NULL;
  bool exit_loop = false;
  size_t* pc = &scheduler->pc[worker_number];

  function_virtual_instruction_t func;
  operand_t op1;
  operand_t op2;
  operand_t op3;
  bool debug;

  while (!exit_loop) {
    func = current_schedule[*pc].func;
    op1 = current_schedule[*pc].op1;
    op2 = current_schedule[*pc].op2;
    op3 = current_schedule[*pc].op3;
    debug = current_schedule[*pc].debug;

    // Execute the current instruction
    func(scheduler->env->platform, worker_number, op1, op2, op3, debug, pc, &returned_reaction, &exit_loop);
  }

  LF_PRINT_DEBUG("Worker %d leaves lf_sched_get_ready_reaction", worker_number);
  return returned_reaction;
}

void StaticScheduler_ctor(StaticScheduler* self, Environment* env, const inst_t** static_schedule) {
  self->env = env;
  self->static_schedule = static_schedule;

  self->super->run = Scheduler_run;
  self->super->do_shutdown_locked = Scheduler_do_shutdown_locked;
  self->super->schedule_at = Scheduler_schedule_at;
  self->super->schedule_at_locked = Scheduler_schedule_at_locked;
  self->super->register_for_cleanup = Scheduler_register_for_cleanup;
  self->super->request_shutdown = Scheduler_request_shutdown;
  self->super->set_and_schedule_start_tag = Scheduler_set_and_schedule_start_tag;
}
